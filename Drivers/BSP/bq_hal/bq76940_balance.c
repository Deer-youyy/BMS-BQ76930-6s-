/* ****************************************************
 * BQ76940 平衡 Provider（BQ76940 硬件适配实现）
 *
 * 本文件是 bq_hal/ 平衡接口的 BQ76940 具体实现，
 * 负责把中性逻辑均衡 mask 翻译成 BQ76940 CELLBAL 寄存器操作。
 *
 * 职责：
 *   本 Provider 是唯一允许直接处理 BQ76940 平衡细节的文件：
 *     - BQ76940_CellBalRegs_t
 *     - BQ76940_BuildSingleCellBalMask / ClearCellBalRegs
 *     - BQ76940_WriteCellBalRegs / ReadCellBalRegs
 *     - CELLBAL1/2/3 与 VC label 位映射
 *   上层（Balance App / bms_tasks）不得出现这些细节。
 *
 * 依赖关系：
 *   Balance App
 *        |
 *   BqHalBalance（中性接口）
 *        |
 *   bq76940_balance.c（本 Provider）
 *        |
 *   BQ76940 Driver
 *        |
 *     Hardware
 * **************************************************** */

#include "bq_hal_balance.h"
#include "bq76940_balance.h"
#include "bq76940_drv.h"

/* VC label 映射表：逻辑下标 i 对应真实的 VC 标签编号。
 * logical 0~8 → VC1,VC2,VC5,VC6,VC7,VC10,VC11,VC12,VC15 */
static const uint8_t s_cell_label[BMS_CELL_COUNT] =
    {
        1U, 2U, 5U, 6U, 7U, 10U, 11U, 12U, 15U};

/* Provider 句柄定义（内部实现细节，外部不可见）*/
struct BqHalBalance
{
    uint32_t model_ver; /* Provider 型号版本 */
};

static struct BqHalBalance s_bq76940_balance;
static struct BqHalBalance *s_default_balance = &s_bq76940_balance;

void BqHalBalance_LoadDefault(void)
{
    s_default_balance = &s_bq76940_balance;
}

BqHalBalance_t *BqHalBalance_Get(void)
{
    return s_default_balance;
}

void BqHalBalanceMask_Clear(BqHalBalanceMask_t *m)
{
    if (m != 0)
    {
        m->logical_mask = 0U;
    }
}

uint8_t BqHalBalanceMask_SetLogical(BqHalBalanceMask_t *m, uint8_t logical_idx)
{
    if (m == 0)
    {
        return BQHAL_BALANCE_ERR_INVALID;
    }

    if (logical_idx >= BMS_CELL_COUNT)
    {
        return BQHAL_BALANCE_ERR_INVALID;
    }

    m->logical_mask |= (1UL << logical_idx);

    return BQHAL_BALANCE_OK;
}

uint8_t BqHalBalanceMask_IsEmpty(const BqHalBalanceMask_t *m)
{
    if (m == 0)
    {
        return 1U;
    }

    return (m->logical_mask == 0U) ? 1U : 0U;
}

void BqHalBalanceMask_Or(BqHalBalanceMask_t *dst, const BqHalBalanceMask_t *src)
{
    if ((dst != 0) && (src != 0))
    {
        dst->logical_mask |= src->logical_mask;
    }
}

uint8_t BqHalBalanceMask_Equal(const BqHalBalanceMask_t *a, const BqHalBalanceMask_t *b)
{
    if ((a == 0) || (b == 0))
    {
        return 0U;
    }

    return (a->logical_mask == b->logical_mask) ? 1U : 0U;
}

uint8_t BqHalBalance_LogicalToLabel(BqHalBalance_t *bal, uint8_t logical_idx)
{
    if (bal == 0)
    {
        return 0U;
    }

    if (logical_idx >= BMS_CELL_COUNT)
    {
        return 0U;
    }

    return s_cell_label[logical_idx];
}

uint8_t BqHalBalance_Write(BqHalBalance_t *bal, const BqHalBalanceMask_t *m)
{
    BQ76940_CellBalRegs_t regs;
    BQ76940_CellBalRegs_t one;
    uint8_t i;
    uint8_t ret;

    if (bal == 0)
    {
        return BQHAL_BALANCE_ERR_INVALID;
    }

    if (m == 0)
    {
        return BQHAL_BALANCE_ERR_INVALID;
    }

    /* 把逻辑 mask 逐位翻译为 VC label，再 OR 进 CELLBAL 寄存器结构 */
    BQ76940_ClearCellBalRegs(&regs);

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        if ((m->logical_mask & (1UL << i)) != 0U)
        {
            BQ76940_ClearCellBalRegs(&one);

            ret = BQ76940_BuildSingleCellBalMask(s_cell_label[i], &one);
            if (ret != BQ76940_OK)
            {
                return BQHAL_BALANCE_ERR_INVALID;
            }

            regs.cellbal1 |= one.cellbal1;
            regs.cellbal2 |= one.cellbal2;
            regs.cellbal3 |= one.cellbal3;
        }
    }

    ret = BQ76940_WriteCellBalRegs(&regs);
    if (ret != BQ76940_OK)
    {
        return BQHAL_BALANCE_ERR_WRITE;
    }

    return BQHAL_BALANCE_OK;
}

uint8_t BqHalBalance_Read(BqHalBalance_t *bal, BqHalBalanceMask_t *m)
{
    BQ76940_CellBalRegs_t regs;
    BQ76940_CellBalRegs_t one;
    uint8_t i;
    uint8_t ret;

    if (bal == 0)
    {
        return BQHAL_BALANCE_ERR_INVALID;
    }

    if (m == 0)
    {
        return BQHAL_BALANCE_ERR_INVALID;
    }

    m->logical_mask = 0U;

    ret = BQ76940_ReadCellBalRegs(&regs);
    if (ret != BQ76940_OK)
    {
        return BQHAL_BALANCE_ERR_READ;
    }

    /* 按 VC label 逐位反向匹配，恢复为逻辑 mask */
    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        BQ76940_ClearCellBalRegs(&one);

        if (BQ76940_BuildSingleCellBalMask(s_cell_label[i], &one) != BQ76940_OK)
        {
            continue;
        }

        if ((one.cellbal1 == (regs.cellbal1 & one.cellbal1)) &&
            (one.cellbal2 == (regs.cellbal2 & one.cellbal2)) &&
            (one.cellbal3 == (regs.cellbal3 & one.cellbal3)))
        {
            m->logical_mask |= (1UL << i);
        }
    }

    return BQHAL_BALANCE_OK;
}

/* BQ76940 兼容性 API：
 * 把中性逻辑 mask 转换回 legacy BQ76940_CellBalRegs_t，
 * 仅用于 AppCtx 兼容字段 / CAN 0x306 快照 / Safe-Off 日志，
 * 不参与 Balance 决策，不暴露给 Neutral Balance API。 */
void BqHalBalance_GetLegacyCellBalRegs(const BqHalBalanceMask_t *mask,
                                       BQ76940_CellBalRegs_t *regs)
{
    BQ76940_CellBalRegs_t one;
    uint8_t i;
    uint8_t ret;

    if (regs == 0)
    {
        return;
    }

    BQ76940_ClearCellBalRegs(regs);

    if (mask == 0)
    {
        return;
    }

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        if ((mask->logical_mask & (1UL << i)) != 0U)
        {
            BQ76940_ClearCellBalRegs(&one);

            ret = BQ76940_BuildSingleCellBalMask(s_cell_label[i], &one);
            if (ret != BQ76940_OK)
            {
                break;
            }

            regs->cellbal1 |= one.cellbal1;
            regs->cellbal2 |= one.cellbal2;
            regs->cellbal3 |= one.cellbal3;
        }
    }
}
