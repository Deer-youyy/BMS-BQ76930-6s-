#ifndef __BQ_HAL_MEASURE_H
#define __BQ_HAL_MEASURE_H

/* ****************************************************
 * BQ 测量 / AFE 状态数据访问层（芯片无关接口）
 *
 * 职责：
 *   只提供 Measurement + AFE Status 的数据访问语义，
 *   不承载 FET / Balance / Protection / CAN / BQ76200 / FreeRTOS / 业务策略。
 *
 * 设计原则：
 *   1. 上层只通过本接口获取中性测量结果，不直接依赖任何具体 BQ 芯片。
 *   2. 中性测量结构不含芯片型号、寄存器地址、芯片原始结构体。
 *   3. 芯片专有“原始载荷”（raw）通过独立的 RawDiag 侧通道输出，
 *      仅供当前芯片的 Print / 调试回填，绝不进入业务语义。
 * **************************************************** */

#include <stdint.h>
#include "../../../Core/bms_types.h"   /* BMS_CELL_COUNT, BmsAfeStatus_t */

/* 不透明句柄：指向具体测量 Provider 实例 */
typedef struct BqHalMeasure BqHalMeasure_t;

/* 中性 ADC 校准（字段与芯片校准一一对应，便于无损映射）。
 * 校准由上层 bring-up 单次读取后传入，避免为本接口新增 I2C 访问。 */
typedef struct
{
    uint16_t gain_uV_per_lsb; /* ADC 增益，单位 uV/lsb */
    int16_t  offset_mV;       /* ADC 偏移，单位 mV */
} BqHalAdcCalib_t;

/* 中性测量语义（本结构不含任何芯片专有原始字段） */
typedef struct
{
    uint16_t cell_mV[BMS_CELL_COUNT]; /* 各逻辑单体电压 mV */
    uint32_t pack_total_mV;           /* 包总压 mV */
    int32_t  pack_current_mA;         /* 包电流 mA */
    uint16_t min_mV;                  /* 最低单体 mV */
    uint16_t max_mV;                  /* 最高单体 mV */
    uint16_t diff_mV;                 /* 单体压差 mV */
    uint8_t  min_label;               /* 最低单体对应逻辑编号 */
    uint8_t  max_label;               /* 最高单体对应逻辑编号 */
    int16_t  ts1_temp_dC;             /* TS1 温度，0.1°C */
    BmsAfeStatus_t afe_status;        /* 中性 AFE 状态语义 */
} BqHalMeasureSample_t;

/* 芯片专有“原始载荷”侧通道（非测量语义，仅供当前芯片 Print 回填） */
typedef struct
{
    uint16_t cell_raw[BMS_CELL_COUNT]; /* 各逻辑单体原始 ADC */
    uint8_t  cc_raw_hi;                /* CC 原始高字节 */
    uint8_t  cc_raw_lo;                /* CC 原始低字节 */
    int16_t  cc_raw_s16;               /* CC 原始有符号值 */
    uint16_t ts1_raw_adc;              /* TS1 原始 ADC */
    uint8_t  sys_stat;                 /* AFE 状态字节 */
} BqHalMeasureRawDiag_t;

/* 中性错误码 */
typedef enum
{
    BQHAL_MEASURE_OK = 0,
    BQHAL_MEASURE_ERR_CELL,     /* 单体电压读取失败 */
    BQHAL_MEASURE_ERR_CC_START, /* CC 启动失败 */
    BQHAL_MEASURE_ERR_CC_WAIT,  /* CC 等待超时 */
    BQHAL_MEASURE_ERR_CC_READ,  /* CC 原始值读取失败 */
    BQHAL_MEASURE_ERR_TS1,      /* TS1 原始值读取失败 */
    BQHAL_MEASURE_ERR_SYS,      /* AFE 状态读取失败 */
    BQHAL_MEASURE_ERR_STATS,    /* 单体统计失败 */
    BQHAL_MEASURE_ERR_CC_CONV,  /* 电流换算失败 */
    BQHAL_MEASURE_ERR_TS1_CONV, /* 温度换算失败 */
    BQHAL_MEASURE_ERR_STATUS    /* 故障掩码解析失败 */
} BqHalMeasureErr_t;

/* 装配：绑定当前默认测量 Provider（由系统装配层 main 调用）。
 * 本接口只暴露中性名，上层不感知具体芯片。 */
void BqHalMeasure_LoadDefault(void);

/* 获取当前已绑定的默认测量 Provider */
BqHalMeasure_t *BqHalMeasure_Get(void);

/* 单次采样：一次硬件读取同时产出中性测量与诊断原始载荷。
 * mea     必须非空；
 * calib   中性校准（必须非空）；
 * m       中性测量输出，必须非空；
 * raw     诊断原始载荷输出，可为空（纯测量消费者无需回填 raw）。
 * 返回 0 = 成功，否则见 BqHalMeasureErr_t。 */
uint8_t BqHalMeasure_ReadSample(BqHalMeasure_t *mea,
                                const BqHalAdcCalib_t *calib,
                                BqHalMeasureSample_t *m,
                                BqHalMeasureRawDiag_t *raw);

#endif /* __BQ_HAL_MEASURE_H */