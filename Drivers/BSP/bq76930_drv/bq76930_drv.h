#ifndef __BQ76930_DRV_H
#define __BQ76930_DRV_H

/* ****************************************************
 * BQ76930 纯芯片驱动（6 串）
 *
 * 本层只做 <芯片寄存器 / I2C / 换算公式> 的封装，
 * 不包含任何业务策略（均衡 / 保护 / SOC / CAN / 串口）。
 * 接口风格对齐正式工程 bq76940_drv。
 *
 * 通信基础：
 *   BQ76930 使用带 PEC 的 I2C 写、普通读。
 *   I2C 物理层统一走正式工程 soft_i2c1。
 * **************************************************** */

#include "sys.h"

/* BQ76930 的 7 位 I2C 地址（与 BQ76940 一致） */
#define BQ76930_I2C_ADDR           0x08

/* ==================== 常用寄存器 ==================== */
#define BQ76930_REG_SYS_STAT       0x00
#define BQ76930_REG_CELLBAL1       0x01
#define BQ76930_REG_CELLBAL2       0x02
#define BQ76930_REG_SYS_CTRL1      0x04
#define BQ76930_REG_SYS_CTRL2      0x05
#define BQ76930_REG_PROTECT1       0x06
#define BQ76930_REG_PROTECT2       0x07
#define BQ76930_REG_PROTECT3       0x08
#define BQ76930_REG_OV_TRIP        0x09
#define BQ76930_REG_UV_TRIP        0x0A
#define BQ76930_REG_CC_CFG         0x0B

/* ==================== 电压采样寄存器（6 串） ==================== */
#define BQ76930_REG_VC1_HI         0x0C
#define BQ76930_REG_VC1_LO         0x0D
#define BQ76930_REG_VC2_HI         0x0E
#define BQ76930_REG_VC2_LO         0x0F
#define BQ76930_REG_VC3_HI         0x10
#define BQ76930_REG_VC3_LO         0x11
#define BQ76930_REG_VC4_HI         0x12
#define BQ76930_REG_VC4_LO         0x13
#define BQ76930_REG_VC5_HI         0x14
#define BQ76930_REG_VC5_LO         0x15
#define BQ76930_REG_VC6_HI         0x16
#define BQ76930_REG_VC6_LO         0x17

/* ==================== 电池组总压 / 温度 / 电流 ==================== */
#define BQ76930_REG_BAT_HI         0x2A
#define BQ76930_REG_BAT_LO         0x2B
#define BQ76930_REG_TS1_HI         0x2C
#define BQ76930_REG_TS1_LO         0x2D
#define BQ76930_REG_TS2_HI         0x2E
#define BQ76930_REG_TS2_LO         0x2F
#define BQ76930_REG_CC_HI          0x32
#define BQ76930_REG_CC_LO          0x33

/* ==================== ADC 校准寄存器 ==================== */
#define BQ76930_REG_ADCGAIN1       0x50
#define BQ76930_REG_ADCOFFSET      0x51
#define BQ76930_REG_ADCGAIN2       0x59

/* ==================== SYS_STAT 位 ==================== */
#define BQ76930_SYS_STAT_CC_READY   (1U << 7)

/* ==================== SYS_CTRL2 位 ==================== */
#define BQ76930_SYS_CTRL2_CHG_ON    (1U << 0)
#define BQ76930_SYS_CTRL2_DSG_ON    (1U << 1)
#define BQ76930_SYS_CTRL2_CC_EN     (1U << 6)
#define BQ76930_SYS_CTRL2_CC_ONESHOT (1U << 5)

/* ==================== 返回码 ==================== */
#define BQ76930_OK                   0
#define BQ76930_ERR_PARAM            1
#define BQ76930_ERR_COMM             2
#define BQ76930_ERR_TIMEOUT          3

/* 6 串电芯数 */
#define BQ76930_CELL_COUNT          6U

/* ==================== 数据类型 ==================== */
/* ADC 校准参数 */
typedef struct
{
    uint16_t gain_uV_per_lsb; /* 每 LSB 增益（uV），参考：GAIN = 365 + ADC_GAIN */
    int16_t  offset_mV;       /* 偏移（mV） */
} BQ76930_AdcCalib_t;

/* CC 电流原始结果 */
typedef struct
{
    uint8_t  raw_hi;
    uint8_t  raw_lo;
    uint16_t raw_u16;
    int16_t  raw_s16;
} BQ76930_CCRaw_t;

/* 6 串单体统计结果 */
typedef struct
{
    uint16_t max_mV;
    uint16_t min_mV;
    uint16_t diff_mV;
    uint8_t  max_cell_index; /* 0~5 */
    uint8_t  min_cell_index; /* 0~5 */
} BQ76930_CellStats_t;

/* ==================== 基础读写 ==================== */
uint8_t BQ76930_ReadReg(uint8_t reg_addr, uint8_t *data);
uint8_t BQ76930_WriteReg(uint8_t reg_addr, uint8_t data);
uint8_t BQ76930_WriteReg_CRC(uint8_t reg_addr, uint8_t data);

/* ==================== 初始化 ==================== */
uint8_t BQ76930_InitForBringUp(void);

/* ==================== 状态寄存器读取 ==================== */
uint8_t BQ76930_ReadSysStat(uint8_t *sys_stat);
uint8_t BQ76930_ClearSysStatBits(uint8_t mask);
uint8_t BQ76930_ReadSysCtrl2(uint8_t *sys_ctrl2);

/* ==================== ADC 校准 ==================== */
uint8_t BQ76930_GetAdcCalib(BQ76930_AdcCalib_t *calib);

/* ==================== 单体电压 ==================== */
uint8_t BQ76930_ReadCellVoltageRaw(uint8_t reg_hi, uint8_t reg_lo, uint16_t *raw_adc);
uint8_t BQ76930_ConvertCellVoltage_mV(uint16_t raw_adc,
                                      const BQ76930_AdcCalib_t *calib,
                                      uint16_t *voltage_mV);
uint8_t BQ76930_ReadCellVoltage_mV(uint8_t cell_index,
                                   const BQ76930_AdcCalib_t *calib,
                                   uint16_t *raw_adc,
                                   uint16_t *voltage_mV);
uint8_t BQ76930_ReadAllCellVoltages_mV(const BQ76930_AdcCalib_t *calib,
                                       uint16_t raw_adc[BQ76930_CELL_COUNT],
                                       uint16_t voltage_mV[BQ76930_CELL_COUNT]);

/* ==================== 总压 ==================== */
uint32_t BQ76930_CalcPackVoltage_mV(const uint16_t voltage_mV[BQ76930_CELL_COUNT]);
uint8_t  BQ76930_AnalyzeCellVoltages(const uint16_t voltage_mV[BQ76930_CELL_COUNT],
                                     BQ76930_CellStats_t *stats);

/* ==================== 电流（CC） ==================== */
uint8_t BQ76930_ReadCCRaw(BQ76930_CCRaw_t *cc);
uint8_t BQ76930_ConvertCurrent_mA(int16_t cc_raw_s16,
                                  uint32_t rsense_uohm,
                                  int32_t *current_mA);

/* ==================== 温度（NTC） ==================== */
uint8_t BQ76930_ReadNtcRaw(uint8_t ts_hi_reg, uint8_t ts_lo_reg, uint16_t *raw_adc);
uint8_t BQ76930_ConvertNtcTemp_dC(uint16_t raw_adc, int16_t *temp_dC);

/* ==================== FET 控制 ==================== */
uint8_t BQ76930_SetFETState(uint8_t chg_on, uint8_t dsg_on);

#endif /* __BQ76930_DRV_H */
