/* ****************************************************
 * BQ76930 硬件适配器（Hardware Adapter）
 *
 * 作用：把芯片无关的 BqHal 能力映射到具体 BQ76930 驱动。
 *      本层允许出现 BQ76930 型号细节，属于 Hardware Adapter 层；
 *      只做能力到驱动 API 的映射，不包含业务策略。
 *
 * 架构：
 *   App / Component
 *        |
 *      BqHal
 *        |
 *   bq76930_hal.c
 *        |
 *   BQ76930 Driver
 *        |
 *     Hardware
 * **************************************************** */

#include "bq_hal.h"
#include "bq76930_drv.h"

/* 不透明句柄内部结构：保留给未来扩展，当前无需额外字段 */
struct BqHal
{
    uint32_t model_ver; /* 适配器版本标识，预留给未来区分具体芯片 */
};

static BqHal_t s_bq76930_hal;
static BqHal_t *s_default_hal = 0U;

/* 初始化 BQ76930 适配器并绑定为默认（由系统装配层 main 调用） */
BqHal_t *BQ76930_HalDefault(void)
{
    BqHal_Bind(&s_bq76930_hal);
    return &s_bq76930_hal;
}

void BqHal_Bind(BqHal_t *hal)
{
    s_default_hal = hal;
}

BqHal_t *BqHal_Get(void)
{
    return s_default_hal;
}

uint8_t BqHal_ApplyFetEn(BqHal_t *hal, uint8_t chg_en, uint8_t dsg_en)
{
    uint8_t sys_ctrl2;

    if (hal == 0U)
    {
        return 1U; /* 未装配适配器，视为失败 */
    }

    /* 读改写 SYS_CTRL2，在适配器内完成 CHG/DSG 使能意图到寄存器的映射 */
    if (BQ76930_ReadReg(BQ76930_REG_SYS_CTRL2, &sys_ctrl2) != BQ76930_OK)
    {
        return BQ76930_ERR_COMM;
    }

    if (chg_en != 0U)
    {
        sys_ctrl2 |= BQ76930_SYS_CTRL2_CHG_ON;
    }
    else
    {
        sys_ctrl2 &= (uint8_t)(~BQ76930_SYS_CTRL2_CHG_ON);
    }

    if (dsg_en != 0U)
    {
        sys_ctrl2 |= BQ76930_SYS_CTRL2_DSG_ON;
    }
    else
    {
        sys_ctrl2 &= (uint8_t)(~BQ76930_SYS_CTRL2_DSG_ON);
    }

    return BQ76930_WriteReg_CRC(BQ76930_REG_SYS_CTRL2, sys_ctrl2);
}