/* ****************************************************
 * BQ76930 纯芯片驱动（6 串）
 *
 * 仅从只读参考源码提取 <寄存器 / I2C / 换算公式> 必需实现，
 * 按正式工程 bq76940_drv 的接口风格裁剪重写。
 *
 * 本文件不含任何业务策略（均衡 / 保护 / SOC / CAN / 串口）。
 * **************************************************** */

#include "bq76930_drv.h"
#include "soft_i2c1.h"
#include "delay.h"
#include <math.h>

/* ==================== 内部辅助函数 ==================== */

/* CRC8/PEC 计算，与 bq76940_drv 同实现（key = 7） */
static uint8_t BQ76930_CRC8(uint8_t *ptr, uint8_t len, uint8_t key)
{
    uint8_t i;
    uint8_t crc = 0;

    while (len-- != 0)
    {
        for (i = 0x80; i != 0; i /= 2)
        {
            if ((crc & 0x80) != 0)
            {
                crc *= 2;
                crc ^= key;
            }
            else
            {
                crc *= 2;
            }

            if ((*ptr & i) != 0)
            {
                crc ^= key;
            }
        }
        ptr++;
    }

    return crc;
}

/* ==================== 基础读写 ==================== */

uint8_t BQ76930_ReadReg(uint8_t reg_addr, uint8_t *data)
{
    if (data == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    return SoftI2C1_ReadReg(BQ76930_I2C_ADDR, reg_addr, data);
}

uint8_t BQ76930_WriteReg(uint8_t reg_addr, uint8_t data)
{
    return SoftI2C1_WriteReg(BQ76930_I2C_ADDR, reg_addr, data);
}

/* 带 PEC 的寄存器写：
 * 发送帧 = [器件地址<<1][寄存器地址][数据][CRC8(前 3 字节, 7)]
 */
uint8_t BQ76930_WriteReg_CRC(uint8_t reg_addr, uint8_t data)
{
    uint8_t tx[4];

    tx[0] = (BQ76930_I2C_ADDR << 1); /* 设备地址 + 写位 0 */
    tx[1] = reg_addr;
    tx[2] = data;
    tx[3] = BQ76930_CRC8(tx, 3, 7);

    SoftI2C1_Start();

    /* 发送器件地址 */
    SoftI2C1_SendByte(tx[0]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 1;
    }

    /* 发送寄存器地址 */
    SoftI2C1_SendByte(tx[1]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 2;
    }

    /* 发送数据 */
    SoftI2C1_SendByte(tx[2]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 3;
    }

    /* 发送 CRC */
    SoftI2C1_SendByte(tx[3]);
    if (SoftI2C1_WaitAck())
    {
        SoftI2C1_Stop();
        return 4;
    }

    SoftI2C1_Stop();

    /* 参考例程每次写后延时，保证芯片处理完成 */
    delay_ms(10);

    return BQ76930_OK;
}


/* ==================== 状态寄存器 ==================== */

uint8_t BQ76930_ReadSysStat(uint8_t *sys_stat)
{
    if (sys_stat == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    return BQ76930_ReadReg(BQ76930_REG_SYS_STAT, sys_stat);
}

uint8_t BQ76930_ClearSysStatBits(uint8_t mask)
{
    return BQ76930_WriteReg_CRC(BQ76930_REG_SYS_STAT, mask);
}

uint8_t BQ76930_ReadSysCtrl2(uint8_t *sys_ctrl2)
{
    if (sys_ctrl2 == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    return BQ76930_ReadReg(BQ76930_REG_SYS_CTRL2, sys_ctrl2);
}

/* ==================== 上电 bring-up / 保护配置 ==================== */

uint8_t BQ76930_ReadBasicRegs(BQ76930_BasicRegs_t *regs)
{
    if (regs == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    if (BQ76930_ReadReg(BQ76930_REG_SYS_STAT,  &regs->sys_stat)  != BQ76930_OK) return 11;
    if (BQ76930_ReadReg(BQ76930_REG_SYS_CTRL1, &regs->sys_ctrl1) != BQ76930_OK) return 12;
    if (BQ76930_ReadReg(BQ76930_REG_SYS_CTRL2, &regs->sys_ctrl2) != BQ76930_OK) return 13;
    if (BQ76930_ReadReg(BQ76930_REG_PROTECT1,  &regs->protect1)  != BQ76930_OK) return 14;
    if (BQ76930_ReadReg(BQ76930_REG_PROTECT2,  &regs->protect2)  != BQ76930_OK) return 15;
    if (BQ76930_ReadReg(BQ76930_REG_PROTECT3,  &regs->protect3)  != BQ76930_OK) return 16;
    if (BQ76930_ReadReg(BQ76930_REG_OV_TRIP,   &regs->ov_trip)   != BQ76930_OK) return 17;
    if (BQ76930_ReadReg(BQ76930_REG_UV_TRIP,   &regs->uv_trip)   != BQ76930_OK) return 18;
    if (BQ76930_ReadReg(BQ76930_REG_CC_CFG,    &regs->cc_cfg)    != BQ76930_OK) return 19;

    return BQ76930_OK;
}

/* 上电最小初始化：按 LOCKED 6S 硬件事实写寄存器表。
 * 与真实源码 BQ_1_config 一致：
 *   SYS_STAT=0xFF  CELLBAL1=0x00  CELLBAL2=0x00  SYS_CTRL1=0x18  SYS_CTRL2=0x43
 *   PROTECT1=0xFF  PROTECT2=0xFF  PROTECT3=0x00  OV_TRIP=0x00    UV_TRIP=0x00
 *   CC_CFG=0x19
 * PROTECT1/PROTECT2 = 0xFF 行为已获批准保持，禁止按手册保留位说明修改。 */
uint8_t BQ76930_InitForBringUp(void)
{
    static const uint8_t reg_table[BQ76930_BASIC_REG_COUNT] =
    {
        BQ76930_REG_SYS_STAT,
        BQ76930_REG_CELLBAL1,
        BQ76930_REG_CELLBAL2,
        BQ76930_REG_SYS_CTRL1,
        BQ76930_REG_SYS_CTRL2,
        BQ76930_REG_PROTECT1,
        BQ76930_REG_PROTECT2,
        BQ76930_REG_PROTECT3,
        BQ76930_REG_OV_TRIP,
        BQ76930_REG_UV_TRIP,
        BQ76930_REG_CC_CFG
    };

    static const uint8_t data_table[BQ76930_BASIC_REG_COUNT] =
    {
        0xFF, 0x00, 0x00, 0x18, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19
    };

    uint8_t i;
    uint8_t ret;

    for (i = 0U; i < BQ76930_BASIC_REG_COUNT; i++)
    {
        ret = BQ76930_WriteReg_CRC(reg_table[i], data_table[i]);
        if (ret != BQ76930_OK)
        {
            return (uint8_t)(2U + i);
        }
    }

    return BQ76930_OK;
}

uint8_t BQ76930_LoadProtectionParams(uint8_t protect3,
                                     uint8_t ov_trip,
                                     uint8_t uv_trip)
{
    uint8_t ret;
    uint8_t rd;

    /* 1. 写 PROTECT3 */
    ret = BQ76930_WriteReg_CRC(BQ76930_REG_PROTECT3, protect3);
    if (ret != BQ76930_OK) return 1;

    ret = BQ76930_ReadReg(BQ76930_REG_PROTECT3, &rd);
    if (ret != BQ76930_OK) return 11;
    if (rd != protect3) return 21;

    /* 2. 写 OV_TRIP */
    ret = BQ76930_WriteReg_CRC(BQ76930_REG_OV_TRIP, ov_trip);
    if (ret != BQ76930_OK) return 2;

    ret = BQ76930_ReadReg(BQ76930_REG_OV_TRIP, &rd);
    if (ret != BQ76930_OK) return 12;
    if (rd != ov_trip) return 22;

    /* 3. 写 UV_TRIP */
    ret = BQ76930_WriteReg_CRC(BQ76930_REG_UV_TRIP, uv_trip);
    if (ret != BQ76930_OK) return 3;

    ret = BQ76930_ReadReg(BQ76930_REG_UV_TRIP, &rd);
    if (ret != BQ76930_OK) return 13;
    if (rd != uv_trip) return 23;

    return BQ76930_OK;
}

uint8_t BQ76930_CalcOvTripFrommV(uint16_t ov_mV,
                                 uint16_t gain_uV_per_lsb,
                                 int16_t offset_mV)
{
    uint32_t full_adc;

    /* full_adc ≈ (ov_mV - offset_mV) / (gain_uV_per_lsb / 1000) */
    (void)gain_uV_per_lsb; /* source OV_UV_1_PROTECT uses fixed 0.377 mV/LSB */
    full_adc = (uint32_t)((((int32_t)ov_mV - (int32_t)offset_mV) * 1000L + 188L) / 377L);

    /* 取 14 位 ADC 值的中间 8 位 */
    return (uint8_t)((full_adc >> 4) & 0xFFU);
}

uint8_t BQ76930_CalcUvTripFrommV(uint16_t uv_mV,
                                 uint16_t gain_uV_per_lsb,
                                 int16_t offset_mV)
{
    uint32_t full_adc;

    (void)gain_uV_per_lsb; /* source OV_UV_1_PROTECT uses fixed 0.377 mV/LSB */
    full_adc = (uint32_t)((((int32_t)uv_mV - (int32_t)offset_mV) * 1000L + 188L) / 377L);

    return (uint8_t)((full_adc >> 4) & 0xFFU);
}

uint8_t BQ76930_ProtectGetActiveFaultMask(uint8_t sys_stat, uint8_t *fault_mask)
{
    if (fault_mask == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    *fault_mask = (uint8_t)(sys_stat & BQ76930_SYS_STAT_HW_LATCH_MASK);

    return BQ76930_OK;
}

uint8_t BQ76930_ProtectClearFaultBits(uint8_t fault_mask)
{
    if (fault_mask == 0U)
    {
        return BQ76930_OK;
    }

    /* SYS_STAT：写 1 清对应位（BQ76930 真实硬件） */
    return BQ76930_WriteReg_CRC(BQ76930_REG_SYS_STAT, fault_mask);
}

/* CHG/DSG 使能控制：读改写 SYS_CTRL2，保留 CC_EN/CC_ONESHOT 语义。 */
uint8_t BQ76930_SetFETState(uint8_t chg_on, uint8_t dsg_on)
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

/* ==================== ADC 校准 ==================== */

uint8_t BQ76930_GetAdcCalib(BQ76930_AdcCalib_t *calib)
{
    uint8_t gain1 = 0;
    uint8_t gain2 = 0;
    uint8_t adc_offset = 0;
    uint8_t adc_gain = 0;

    if (calib == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    if (BQ76930_ReadReg(BQ76930_REG_ADCGAIN1, &gain1) != BQ76930_OK) return 2;
    if (BQ76930_ReadReg(BQ76930_REG_ADCGAIN2, &gain2) != BQ76930_OK) return 3;
    if (BQ76930_ReadReg(BQ76930_REG_ADCOFFSET, &adc_offset) != BQ76930_OK) return 4;

    /* 位拼接与增益公式取自参考 Get_offset() */
    adc_gain = (uint8_t)(((gain1 & 0x0C) << 1) + ((gain2 & 0xE0) >> 5));
    calib->gain_uV_per_lsb = (uint16_t)(365 + adc_gain);
    calib->offset_mV = (int16_t)((int8_t)adc_offset);

    return BQ76930_OK;
}

/* ==================== 单体电压 ==================== */

uint8_t BQ76930_ReadCellVoltageRaw(uint8_t reg_hi, uint8_t reg_lo, uint16_t *raw_adc)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (raw_adc == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    if (BQ76930_ReadReg(reg_hi, &hi) != BQ76930_OK) return 2;
    if (BQ76930_ReadReg(reg_lo, &lo) != BQ76930_OK) return 3;

    *raw_adc = ((uint16_t)hi << 8) | lo;

    return BQ76930_OK;
}

uint8_t BQ76930_ConvertCellVoltage_mV(uint16_t raw_adc,
                                      const BQ76930_AdcCalib_t *calib,
                                      uint16_t *voltage_mV)
{
    int32_t temp_mV = 0;

    if ((calib == 0U) || (voltage_mV == 0U))
    {
        return BQ76930_ERR_PARAM;
    }

    /* cell_mV = (raw * GAIN(uV) / 1000) + offset(mV) */
    temp_mV = (int32_t)(((uint32_t)raw_adc * calib->gain_uV_per_lsb) / 1000U);
    temp_mV += (int32_t)calib->offset_mV;

    if (temp_mV < 0)
    {
        temp_mV = 0;
    }
    else if (temp_mV > 65535)
    {
        temp_mV = 65535;
    }

    *voltage_mV = (uint16_t)temp_mV;

    return BQ76930_OK;
}

uint8_t BQ76930_ReadCellVoltage_mV(uint8_t cell_index,
                                   const BQ76930_AdcCalib_t *calib,
                                   uint16_t *raw_adc,
                                   uint16_t *voltage_mV)
{
    /* 真实 6S 硬件映射：逻辑单体 i → VC label。
     * 逻辑 0~5 → VC1,VC2,VC5,VC6,VC7,VC10
     * 禁止按连续 VC1~VC6 读取。 */
    static const uint8_t vc_hi[BQ76930_CELL_COUNT] =
    {
        BQ76930_REG_VC1_HI,
        BQ76930_REG_VC2_HI,
        BQ76930_REG_VC5_HI,
        BQ76930_REG_VC6_HI,
        BQ76930_REG_VC7_HI,
        BQ76930_REG_VC10_HI
    };

    static const uint8_t vc_lo[BQ76930_CELL_COUNT] =
    {
        BQ76930_REG_VC1_LO,
        BQ76930_REG_VC2_LO,
        BQ76930_REG_VC5_LO,
        BQ76930_REG_VC6_LO,
        BQ76930_REG_VC7_LO,
        BQ76930_REG_VC10_LO
    };

    uint8_t ret = 0;
    uint16_t local_raw = 0;

    if ((calib == 0U) || (raw_adc == 0U) || (voltage_mV == 0U))
    {
        return BQ76930_ERR_PARAM;
    }

    if (cell_index >= BQ76930_CELL_COUNT)
    {
        return BQ76930_ERR_PARAM;
    }

    ret = BQ76930_ReadCellVoltageRaw(vc_hi[cell_index], vc_lo[cell_index], &local_raw);
    if (ret != BQ76930_OK)
    {
        return (uint8_t)(2 + cell_index);
    }

    ret = BQ76930_ConvertCellVoltage_mV(local_raw, calib, voltage_mV);
    if (ret != BQ76930_OK)
    {
        return 8;
    }

    *raw_adc = local_raw;

    return BQ76930_OK;
}

uint8_t BQ76930_ReadAllCellVoltages_mV(const BQ76930_AdcCalib_t *calib,
                                       uint16_t raw_adc[BQ76930_CELL_COUNT],
                                       uint16_t voltage_mV[BQ76930_CELL_COUNT])
{
    uint8_t i;
    uint8_t ret = 0;

    if ((calib == 0U) || (raw_adc == 0U) || (voltage_mV == 0U))
    {
        return BQ76930_ERR_PARAM;
    }

    for (i = 0; i < BQ76930_CELL_COUNT; i++)
    {
        ret = BQ76930_ReadCellVoltage_mV(i, calib, &raw_adc[i], &voltage_mV[i]);
        if (ret != BQ76930_OK)
        {
            return (uint8_t)(10 + i);
        }
    }

    return BQ76930_OK;
}

/* ==================== 总压 ==================== */

uint32_t BQ76930_CalcPackVoltage_mV(const uint16_t voltage_mV[BQ76930_CELL_COUNT])
{
    uint8_t i;
    uint32_t total_mV = 0;

    if (voltage_mV == 0U)
    {
        return 0U;
    }

    for (i = 0; i < BQ76930_CELL_COUNT; i++)
    {
        total_mV += voltage_mV[i];
    }

    return total_mV;
}

uint8_t BQ76930_AnalyzeCellVoltages(const uint16_t voltage_mV[BQ76930_CELL_COUNT],
                                    BQ76930_CellStats_t *stats)
{
    /* 真实 6S 硬件映射：逻辑下标 i → VC label */
    static const uint8_t cell_label[BQ76930_CELL_COUNT] =
    {
        1U, 2U, 5U, 6U, 7U, 10U
    };

    uint8_t i;
    uint8_t max_idx = 0;
    uint8_t min_idx = 0;

    if ((voltage_mV == 0U) || (stats == 0U))
    {
        return BQ76930_ERR_PARAM;
    }

    for (i = 1; i < BQ76930_CELL_COUNT; i++)
    {
        if (voltage_mV[i] > voltage_mV[max_idx])
        {
            max_idx = i;
        }

        if (voltage_mV[i] < voltage_mV[min_idx])
        {
            min_idx = i;
        }
    }

    stats->max_mV = voltage_mV[max_idx];
    stats->min_mV = voltage_mV[min_idx];
    stats->diff_mV = (uint16_t)(stats->max_mV - stats->min_mV);
    stats->max_cell_label = cell_label[max_idx];
    stats->min_cell_label = cell_label[min_idx];

    return BQ76930_OK;
}

/* ==================== 电流（CC） ==================== */

uint8_t BQ76930_ReadCCRaw(BQ76930_CCRaw_t *cc)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (cc == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    if (BQ76930_ReadReg(BQ76930_REG_CC_HI, &hi) != BQ76930_OK) return 2;
    if (BQ76930_ReadReg(BQ76930_REG_CC_LO, &lo) != BQ76930_OK) return 3;

    cc->raw_hi  = hi;
    cc->raw_lo  = lo;
    cc->raw_u16 = ((uint16_t)hi << 8) | (uint16_t)lo;
    cc->raw_s16 = (int16_t)(cc->raw_u16);

    return BQ76930_OK;
}

uint8_t BQ76930_ConvertCurrent_mA(int16_t cc_raw_s16,
                                  uint32_t rsense_uohm,
                                  int32_t *current_mA)
{
    int32_t temp_mA;

    if ((current_mA == 0U) || (rsense_uohm == 0U))
    {
        return BQ76930_ERR_PARAM;
    }

    /* CC_LSB ≈ 8.44 uV/LSB；current_mA = cc_raw * 8440 / rsense(μΩ) */
    temp_mA = ((int32_t)cc_raw_s16 * 8440) / (int32_t)rsense_uohm;

    *current_mA = temp_mA;

    return BQ76930_OK;
}

/* ==================== 温度（NTC） ==================== */

uint8_t BQ76930_ReadNtcRaw(uint8_t ts_hi_reg, uint8_t ts_lo_reg, uint16_t *raw_adc)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (raw_adc == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    if (BQ76930_ReadReg(ts_hi_reg, &hi) != BQ76930_OK) return 2;
    if (BQ76930_ReadReg(ts_lo_reg, &lo) != BQ76930_OK) return 3;

    *raw_adc = ((uint16_t)hi << 8) | lo;

    return BQ76930_OK;
}

uint8_t BQ76930_ConvertNtcTemp_dC(uint16_t raw_adc, int16_t *temp_dC)
{
    float vts_mV;
    float rt_ntc;
    float temp_C;

    const float Rp = 10000.0f;        /* 分压电阻 10k */
    const float Bx = 3380.0f;         /* NTC B 值 */
    const float T2 = 273.15f + 25.0f; /* 25°C 对应开尔文 */
    const float Ka = 273.15f;

    if (temp_dC == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    /* TS 通道电压 = raw * 382uV/LSB（转 mV） */
    vts_mV = (float)raw_adc * 382.0f / 1000.0f;

    if ((3300.0f - vts_mV) <= 1.0f)
    {
        return BQ76930_ERR_PARAM;
    }

    /* 由分压反推 NTC 电阻 */
    rt_ntc = (10000.0f * vts_mV) / (3300.0f - vts_mV);

    if (rt_ntc <= 0.0f)
    {
        return BQ76930_ERR_PARAM;
    }

    /* B 参数公式 */
    temp_C = 1.0f / (1.0f / T2 + logf(rt_ntc / Rp) / Bx) - Ka;

    /* 转 0.1°C */
    *temp_dC = (int16_t)(temp_C * 10.0f + (temp_C >= 0 ? 0.5f : -0.5f));

    return BQ76930_OK;
}

/* ==================== 均衡（CELLBAL） ==================== */

uint8_t BQ76930_ReadCellBalRegs(BQ76930_CellBalRegs_t *regs)
{
    if (regs == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    if (BQ76930_ReadReg(BQ76930_REG_CELLBAL1, &regs->cellbal1) != BQ76930_OK) return 2;
    if (BQ76930_ReadReg(BQ76930_REG_CELLBAL2, &regs->cellbal2) != BQ76930_OK) return 3;

    return BQ76930_OK;
}

uint8_t BQ76930_WriteCellBalRegs(const BQ76930_CellBalRegs_t *regs)
{
    if (regs == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    /* CELLBAL 为配置类寄存器，写操作必须带 PEC（与参考例程一致） */
    if (BQ76930_WriteReg_CRC(BQ76930_REG_CELLBAL1, regs->cellbal1) != BQ76930_OK) return 2;
    if (BQ76930_WriteReg_CRC(BQ76930_REG_CELLBAL2, regs->cellbal2) != BQ76930_OK) return 3;

    return BQ76930_OK;
}

void BQ76930_ClearCellBalRegs(BQ76930_CellBalRegs_t *regs)
{
    if (regs == 0U)
    {
        return;
    }

    regs->cellbal1 = 0x00;
    regs->cellbal2 = 0x00;
}

/* 根据真实电芯编号（VC label，1/2/5/6/7/10）把对应均衡位置 1。
 * 真实 6S 硬件映射：
 *   逻辑 1  → CB1  → CELLBAL1 bit0
 *   逻辑 2  → CB2  → CELLBAL1 bit1
 *   逻辑 5  → CB5  → CELLBAL1 bit4
 *   逻辑 6  → CB6  → CELLBAL2 bit0
 *   逻辑 7  → CB7  → CELLBAL2 bit1
 *   逻辑 10 → CB10 → CELLBAL2 bit4
 * 禁止按逻辑 Cell1~6 直接映射 bit0~bit5。 */
uint8_t BQ76930_BuildSingleCellBalMask(uint8_t cell_label,
                                       BQ76930_CellBalRegs_t *regs)
{
    if (regs == 0U)
    {
        return BQ76930_ERR_PARAM;
    }

    switch (cell_label)
    {
    case 1:
        regs->cellbal1 |= 0x01U; /* CELLBAL1 bit0 */
        break;
    case 2:
        regs->cellbal1 |= 0x02U; /* CELLBAL1 bit1 */
        break;
    case 5:
        regs->cellbal1 |= 0x10U; /* CELLBAL1 bit4 */
        break;
    case 6:
        regs->cellbal2 |= 0x01U; /* CELLBAL2 bit0 */
        break;
    case 7:
        regs->cellbal2 |= 0x02U; /* CELLBAL2 bit1 */
        break;
    case 10:
        regs->cellbal2 |= 0x10U; /* CELLBAL2 bit4 */
        break;
    default:
        return BQ76930_ERR_PARAM;
    }

    return BQ76930_OK;
}

/* 源码 OCD_SCD_PROTECT(): PROTECT1/2 = 0xFF (SCD 66A/33mV/400us, OCD 100A/50mV/1280ms). */
uint8_t BQ76930_LoadOcdScdProtection(void)
{
    if (BQ76930_WriteReg_CRC(BQ76930_REG_PROTECT1, 0xFF) != BQ76930_OK)
    {
        return 2;
    }
    if (BQ76930_WriteReg_CRC(BQ76930_REG_PROTECT2, 0xFF) != BQ76930_OK)
    {
        return 3;
    }
    return BQ76930_OK;
}
