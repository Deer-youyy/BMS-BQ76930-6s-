/* BQ76930 Balance Provider (real 6S hardware).
 *
 * Maps the neutral logical balance mask (bit i = logical cell i) onto
 * the BQ76930 CELLBAL1 / CELLBAL2 registers:
 *   logical 1 -> CB1  -> CELLBAL1 bit0
 *   logical 2 -> CB2  -> CELLBAL1 bit1
 *   logical 5 -> CB5  -> CELLBAL1 bit4
 *   logical 6 -> CB6  -> CELLBAL2 bit0
 *   logical 7 -> CB7  -> CELLBAL2 bit1
 *   logical 10-> CB10 -> CELLBAL2 bit4
 * The BQ76930 has no CELLBAL3 register (unlike BQ76940).
 */

#include "bq_hal_balance.h"
#include "bq76930_balance.h"
#include "bq76930_drv.h"

/* logical index i -> real VC label number. */
static const uint8_t s_cell_label[BMS_CELL_COUNT] =
    {1U, 2U, 5U, 6U, 7U, 10U};

/* Provider internal state (opaque). */
struct BqHalBalance
{
    uint32_t model_ver;
};

static struct BqHalBalance s_bq76930_balance;
static struct BqHalBalance *s_default_balance = &s_bq76930_balance;

void BqHalBalance_LoadDefault(void)
{
    s_default_balance = &s_bq76930_balance;
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
    BQ76930_CellBalRegs_t regs;
    BQ76930_CellBalRegs_t one;
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

    /* Build the CELLBAL registers from the logical mask. */
    BQ76930_ClearCellBalRegs(&regs);

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        if ((m->logical_mask & (1UL << i)) != 0U)
        {
            BQ76930_ClearCellBalRegs(&one);

            ret = BQ76930_BuildSingleCellBalMask(s_cell_label[i], &one);
            if (ret != BQ76930_OK)
            {
                return BQHAL_BALANCE_ERR_INVALID;
            }

            regs.cellbal1 |= one.cellbal1;
            regs.cellbal2 |= one.cellbal2;
        }
    }

    ret = BQ76930_WriteCellBalRegs(&regs);
    if (ret != BQ76930_OK)
    {
        return BQHAL_BALANCE_ERR_WRITE;
    }

    return BQHAL_BALANCE_OK;
}

uint8_t BqHalBalance_Read(BqHalBalance_t *bal, BqHalBalanceMask_t *m)
{
    BQ76930_CellBalRegs_t regs;
    BQ76930_CellBalRegs_t one;
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

    ret = BQ76930_ReadCellBalRegs(&regs);
    if (ret != BQ76930_OK)
    {
        return BQHAL_BALANCE_ERR_READ;
    }

    /* Reconstruct the logical mask from the CELLBAL registers. */
    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        BQ76930_ClearCellBalRegs(&one);

        if (BQ76930_BuildSingleCellBalMask(s_cell_label[i], &one) != BQ76930_OK)
        {
            continue;
        }

        if ((one.cellbal1 == (regs.cellbal1 & one.cellbal1)) &&
            (one.cellbal2 == (regs.cellbal2 & one.cellbal2)))
        {
            m->logical_mask |= (1UL << i);
        }
    }

    return BQHAL_BALANCE_OK;
}

/* BQ76930-specific legacy API.
 * Converts a neutral logical mask into the BQ76930 CELLBAL registers
 * (no CELLBAL3). Used for AppCtx snapshot / CAN 0x306 / Safe-Off. */
void BqHalBalance_GetLegacyCellBalRegs(const BqHalBalanceMask_t *mask,
                                       BQ76930_CellBalRegs_t *regs)
{
    BQ76930_CellBalRegs_t one;
    uint8_t i;
    uint8_t ret;

    if (regs == 0)
    {
        return;
    }

    BQ76930_ClearCellBalRegs(regs);

    if (mask == 0)
    {
        return;
    }

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        if ((mask->logical_mask & (1UL << i)) != 0U)
        {
            BQ76930_ClearCellBalRegs(&one);

            ret = BQ76930_BuildSingleCellBalMask(s_cell_label[i], &one);
            if (ret != BQ76930_OK)
            {
                break;
            }

            regs->cellbal1 |= one.cellbal1;
            regs->cellbal2 |= one.cellbal2;
        }
    }
}
