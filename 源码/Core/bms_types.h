/**
 * bms_types.h - BMS 6S Domain 数据模型（Migration M01）
 *
 * 说明：
 *  - 本文件定义 BMS 领域层最小数据模型。业务代码只应访问 Domain Cell 0..5，
 *    不得感知 BQ76930 的稀疏 VC 通道位置。
 *  - M01 仅建立数据模型，不建立 M02/M03/M04 的完整 Service 架构。
 */
#ifndef BMS_TYPES_H
#define BMS_TYPES_H

#include <stdint.h>

/* 6S 电池串有效电芯数量 */
#define BMS_CELL_COUNT 6U

/* 一次采样的 6S 电池测量结果（Domain 层视图） */
typedef struct
{
    uint16_t cell_mv[BMS_CELL_COUNT]; /* 6 个有效电芯电压，单位 mV */
    uint32_t pack_mv;                 /* 总电压，单位 mV（6 个有效电芯之和） */
    int32_t  current_ma;              /* 电流，单位 mA */
    int16_t  temperature_dC;          /* 温度，单位 0.1℃ */
    uint8_t  estimated_soc_percent;   /* SOC 估算，单位 % */
    uint32_t sample_tick;             /* 采样时刻（tick） */
    uint8_t  valid;                   /* 本帧数据有效标志 */
} BmsMeasurement_t;

/* 故障状态（M01 最小模型，字段与当前 main.c 保护逻辑对应） */
typedef struct
{
    uint8_t ov;  /* 过压 */
    uint8_t uv;  /* 欠压 */
    uint8_t oc;  /* 过流 */
    uint8_t ot;  /* 过温 */
} BmsFaultState_t;

/* 控制状态（M01 最小模型，与当前 MOS/FET 控制对应） */
typedef struct
{
    uint8_t chg_fet;  /* 充电 MOS 状态 */
    uint8_t dsg_fet;  /* 放电 MOS 状态 */
} BmsControlState_t;

#endif /* BMS_TYPES_H */
