#ifndef __BMS_SERVICE_H
#define __BMS_SERVICE_H

/**
 * Migration-005B: Task -> neutral BMS Service boundary header.
 *
 * 本头文件定义 Task 层与 BMS Service 之间的中性接口：
 * - 不包含任何 BQ76930 / BQ76940 App 头文件；
 * - 不暴露 BQ76940_AppCtx_t 完整类型（仅前置声明）；
 * - 不暴露任何 BQ76930 专属类型（AdcCalib / CCRaw / HalFaultDecode 均已中性化）；
 * - 采样数据使用中性 BMS_SampleData_t / BMS_CellStats_t / BMS_CCRaw_t / BMS_AdcCalib_t，
 *   Service 内部与 legacy BQ76940 / BQ76930 类型逐字段映射
 *   （禁止类型强转 / sizeof / offsetof / union alias / memcpy）。
 */

#include <stdint.h>
#include "../../Core/bms_types.h"

/* Legacy App 前置声明，避免暴露完整 BQ76940_AppCtx_t。 */
struct BQ76940_AppCtx;

/* BMS Service 上下文句柄（不透明，实际定义位于 bms_service.c）。 */
typedef struct BMS_ServiceContext BMS_ServiceContext_t;

/* 中性 ADC 校准（逐字段映射自 ADC calibration）。 */
typedef struct
{
    uint16_t gain_uV_per_lsb;
    int16_t  offset_mV;
} BMS_AdcCalib_t;

/* 中性 CC 原始值（逐字段映射自 CC raw）。 */
typedef struct
{
    uint8_t  raw_hi;
    uint8_t  raw_lo;
    uint16_t raw_u16;
    int16_t  raw_s16;
} BMS_CCRaw_t;

/* 中性导电池芯统计（逐字段映射自 BQ76940_CellStats9_t）。 */
typedef struct
{
    uint16_t max_mV;
    uint16_t min_mV;
    uint16_t diff_mV;
    uint8_t  max_cell_label;
    uint8_t  min_cell_label;
} BMS_CellStats_t;

/* 中性采样数据（逐字段映射自 BQ76940_AppSampleData_t）。 */
typedef struct
{
    uint16_t cell_raw[BMS_CELL_COUNT];
    uint16_t cell_mV[BMS_CELL_COUNT];
    uint32_t pack_total_mV;
    BMS_CellStats_t cell_stats;
    BMS_CCRaw_t cc_raw;
    int32_t pack_current_mA;
    int8_t  pack_current_dir;
    uint16_t ts1_raw_adc;
    int16_t  ts1_temp_dC;
    uint8_t  sys_stat;
    uint8_t  fault_mask_active;
} BMS_SampleData_t;

/* 中性硬件故障状态（逐字段映射自 hardware fault decode）。
 * current_fault_active == (ocd || scd)；voltage_fault_active == (ov || uv)。 */
typedef struct
{
    uint8_t ocd;
    uint8_t scd;
    uint8_t ov;
    uint8_t uv;
    uint8_t device_not_ready;
    uint8_t override_alert;
    uint8_t cc_ready;
    uint8_t current_fault_active;
    uint8_t voltage_fault_active;
} BMS_HwFaultState_t;

/* ---- Context：装配层唯一入口，保存 Legacy App 指针 ---- */
BMS_ServiceContext_t *BMS_ServiceInit(struct BQ76940_AppCtx *legacy);

/* ---- Sample ---- */
void    BMS_ServiceGetCalib(BMS_ServiceContext_t *svc, BMS_AdcCalib_t *calib);
uint8_t BMS_ServiceSampleReadHw(const BMS_AdcCalib_t *calib,
                                BMS_SampleData_t *sample);
uint8_t BMS_ServiceSampleProcess(BMS_SampleData_t *sample);
uint8_t BMS_ServiceSampleCommit(BMS_ServiceContext_t *svc,
                                const BMS_SampleData_t *sample,
                                uint8_t *notify_protect);
void    BMS_ServiceSampleReportFail(BMS_ServiceContext_t *svc,
                                    uint8_t fault_code,
                                    uint8_t fault_stage,
                                    uint8_t ret,
                                    uint8_t *enter_fault);

/* ---- Hardware Fault：中性 SYS_STAT 解码（纯函数，无锁） ---- */
void BMS_ServiceHwFaultDecode(uint8_t sys_stat, BMS_HwFaultState_t *fault_state);

/* ---- Runtime ---- */
void    BMS_ServiceRuntimeTakeSafeOff(BMS_ServiceContext_t *svc,
                                      uint8_t *need_safe_off);
void    BMS_ServiceRuntimeForceExternalOff(BMS_ServiceContext_t *svc);
uint8_t BMS_ServiceRuntimeAfeOffHw(void);
void    BMS_ServiceRuntimeAfeOffCommit(BMS_ServiceContext_t *svc,
                                       uint8_t safe_off_result,
                                       uint8_t *retry_allowed);

/* ---- Control ---- */
uint8_t BMS_ServiceControlUpdate(BMS_ServiceContext_t *svc);

#endif /* __BMS_SERVICE_H */
