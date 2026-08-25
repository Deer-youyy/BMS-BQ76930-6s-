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
#include "bq76930_hal.h"
#include "delay.h"

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
/* ==================== BQ76930 Provider layer (bq76930_hal.h) ==================== */

uint8_t BQ76930_HalApplyFetEn(uint8_t chg_en, uint8_t dsg_en)
{
    return BQ76930_SetFETState(chg_en, dsg_en);
}

uint8_t BQ76930_HalSetCHG(uint8_t chg_on)
{
    uint8_t sys_ctrl2;

    if (BQ76930_ReadReg(BQ76930_REG_SYS_CTRL2, &sys_ctrl2) != BQ76930_OK)
    {
        return BQ76930_ERR_COMM;
    }

    if (chg_on != 0U)
    {
        sys_ctrl2 |= BQ76930_SYS_CTRL2_CHG_ON;
    }
    else
    {
        sys_ctrl2 &= (uint8_t)(~BQ76930_SYS_CTRL2_CHG_ON);
    }

    return BQ76930_WriteReg_CRC(BQ76930_REG_SYS_CTRL2, sys_ctrl2);
}

uint8_t BQ76930_HalSetDSG(uint8_t dsg_on)
{
    uint8_t sys_ctrl2;

    if (BQ76930_ReadReg(BQ76930_REG_SYS_CTRL2, &sys_ctrl2) != BQ76930_OK)
    {
        return BQ76930_ERR_COMM;
    }

    if (dsg_on != 0U)
    {
        sys_ctrl2 |= BQ76930_SYS_CTRL2_DSG_ON;
    }
    else
    {
        sys_ctrl2 &= (uint8_t)(~BQ76930_SYS_CTRL2_DSG_ON);
    }

    return BQ76930_WriteReg_CRC(BQ76930_REG_SYS_CTRL2, sys_ctrl2);
}

uint8_t BQ76930_HalReadSysStat(uint8_t *sys_stat)
{
    return BQ76930_ReadSysStat(sys_stat);
}

uint8_t BQ76930_HalReadSysCtrl2(uint8_t *sys_ctrl2)
{
    return BQ76930_ReadSysCtrl2(sys_ctrl2);
}

void BQ76930_HalDecodeSysStat(uint8_t sys_stat, BQ76930_HalFaultDecode_t *dec)
{
    if (dec == 0U)
    {
        return;
    }

    dec->ocd             = (sys_stat & BQ76930_SYS_STAT_OCD)            ? 1U : 0U;
    dec->scd             = (sys_stat & BQ76930_SYS_STAT_SCD)            ? 1U : 0U;
    dec->ov              = (sys_stat & BQ76930_SYS_STAT_OV)             ? 1U : 0U;
    dec->uv              = (sys_stat & BQ76930_SYS_STAT_UV)             ? 1U : 0U;
    dec->device_xready   = (sys_stat & BQ76930_SYS_STAT_DEVICE_XREADY)  ? 1U : 0U;
    dec->ovrd_alert      = (sys_stat & BQ76930_SYS_STAT_OVRD_ALERT)     ? 1U : 0U;
    dec->cc_ready        = (sys_stat & BQ76930_SYS_STAT_CC_READY)       ? 1U : 0U;
    dec->hw_latch_mask    = (uint8_t)(sys_stat & BQ76930_SYS_STAT_HW_LATCH_MASK);
    dec->current_fault_mask = (uint8_t)(sys_stat & BQ76930_SYS_STAT_CURRENT_FAULT_MASK);
    dec->voltage_fault_mask = (uint8_t)(sys_stat & BQ76930_SYS_STAT_VOLTAGE_FAULT_MASK);
}

uint8_t BQ76930_HalFaultLatchMask(uint8_t sys_stat)
{
    return (uint8_t)(sys_stat & BQ76930_SYS_STAT_HW_LATCH_MASK);
}

uint8_t BQ76930_HalInit(void)
{
    uint8_t ret;

    ret = BQ76930_InitForBringUp();
    if (ret != BQ76930_OK)
    {
        return ret;
    }

    delay_ms(20);

    return BQ76930_OK;
}

uint8_t BQ76930_HalReadBasicRegs(BQ76930_BasicRegs_t *regs)
{
    return BQ76930_ReadBasicRegs(regs);
}

uint8_t BQ76930_HalGetAdcCalib(BQ76930_AdcCalib_t *calib)
{
    return BQ76930_GetAdcCalib(calib);
}

uint8_t BQ76930_HalLoadProtection(uint8_t protect3,
                                  uint16_t ov_target_mV,
                                  uint16_t uv_target_mV,
                                  const BQ76930_AdcCalib_t *calib)
{
    uint8_t ov_trip;
    uint8_t uv_trip;

    if (calib == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    ov_trip = BQ76930_CalcOvTripFrommV(ov_target_mV,
                                       calib->gain_uV_per_lsb,
                                       calib->offset_mV);
    uv_trip = BQ76930_CalcUvTripFrommV(uv_target_mV,
                                       calib->gain_uV_per_lsb,
                                       calib->offset_mV);

    return BQ76930_LoadProtectionParams(protect3, ov_trip, uv_trip);
}

uint8_t BQ76930_HalLoadOcdScdProtection(void)
{
    return BQ76930_LoadOcdScdProtection();
}

uint8_t BQ76930_HalReadCellBalRegs(BQ76930_CellBalRegs_t *regs)
{
    return BQ76930_ReadCellBalRegs(regs);
}
