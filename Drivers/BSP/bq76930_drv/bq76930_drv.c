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
    static const uint8_t vc_hi[BQ76930_CELL_COUNT] =
    {
        BQ76930_REG_VC1_HI,
        BQ76930_REG_VC2_HI,
        BQ76930_REG_VC3_HI,
        BQ76930_REG_VC4_HI,
        BQ76930_REG_VC5_HI,
        BQ76930_REG_VC6_HI
    };

    static const uint8_t vc_lo[BQ76930_CELL_COUNT] =
    {
        BQ76930_REG_VC1_LO,
        BQ76930_REG_VC2_LO,
        BQ76930_REG_VC3_LO,
        BQ76930_REG_VC4_LO,
        BQ76930_REG_VC5_LO,
        BQ76930_REG_VC6_LO
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
    stats->max_cell_index = max_idx;
    stats->min_cell_index = min_idx;

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

