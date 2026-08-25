#ifndef __BMS_TYPES_H
#define __BMS_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Core 命名空间：6S 领域纯类型
 *
 * 本头文件只定义“硬件无关”的领域类型，供 App / Service / Business 层复用。
 * 硬件相关常量（寄存器、换算系数等）一律不进入本文件，
 * 由 Drivers/BSP 层各自持有。
 * ========================================================================= */

/* 目标硬件事实：BQ76930 / 6S，6 串逻辑单体 */
#define BMS_CELL_COUNT 6U

/* BQ76200 legacy exec layer: 0=isolated (no BQ76200 hw, CHG/DSG via BQ76930 SYS_CTRL2) */
#define BQ76200_LEGACY_ENABLE 0U

/* 采样测量快照（硬件无关领域类型）*/
typedef struct
{
    uint16_t cell_mv[BMS_CELL_COUNT]; /* 9 个单体电压，单位 mV */
    uint32_t pack_mv;                 /* 总电压，单位 mV */
    int32_t current_ma;               /* 电流，单位 mA */
    int16_t temperature_dC;           /* 温度，单位 0.1 ℃ */
    uint8_t estimated_soc_percent;    /* SOC 估算，单位 % */
    uint32_t sample_tick;             /* 采样时刻（tick） */
    uint8_t valid;                    /* 本帧数据有效标志 */
} BmsMeasurement_t;

/* 故障状态（硬件无关领域类型）*/
typedef struct
{
    uint8_t ov; /* 过压 */
    uint8_t uv; /* 欠压 */
    uint8_t oc; /* 过流 */
    uint8_t ot; /* 过温 */
} BmsFaultState_t;

/* 执行控制状态（硬件无关领域类型）*/
typedef struct
{
    uint8_t chg_fet; /* 充电 MOS 状态 */
    uint8_t dsg_fet; /* 放电 MOS 状态 */
} BmsControlState_t;

/* AFE 状态（硬件无关领域类型）*/
typedef struct
{
    uint8_t fault_mask_active; /* 当前激活的硬件故障位（中性语义，数值同采样层现有 fault_mask_active） */
} BmsAfeStatus_t;

#ifdef __cplusplus
}
#endif

#endif /* __BMS_TYPES_H */
