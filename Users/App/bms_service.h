#ifndef __BMS_SERVICE_H
#define __BMS_SERVICE_H

/**
 * Migration-005A: Task -> neutral BMS Service boundary header.
 *
 * 本头文件定义 Task 层与 BMS Service 之间的中性接口：
 * - 不包含任何 BQ76940 App 头文件；
 * - 不暴露 BQ76940_AppCtx 完整类型（仅前置声明）；
 * - 采样数据使用中性 BMS_SampleData_t / BMS_CellStats_t，
 *   Service 内部与 legacy BQ76940_AppSampleData_t / BQ76940_CellStats9_t
 *   逐字段映射（禁止类型强转 / sizeof / offsetof / union alias / memcpy）；
 * - BQ76930_AdcCalib_t 为明确记录的 Deferred Coupling（005B 再做中性化）。
 */

#include <stdint.h>
#include "../../Core/bms_types.h"
#include "bq76930_hal.h"

/* Legacy App 前置声明，避免暴露完整 BQ76940_AppCtx。 */
struct BQ76940_AppCtx;

/* BMS Service 上下文句柄（不透明，实际定义位于 bms_service.c）。 */
typedef struct BMS_ServiceContext BMS_ServiceContext_t;

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
    BQ76930_CCRaw_t cc_raw;
    int32_t pack_current_mA;
    int8_t  pack_current_dir;
    uint16_t ts1_raw_adc;
    int16_t  ts1_temp_dC;
    uint8_t  sys_stat;
    uint8_t  fault_mask_active;
} BMS_SampleData_t;

/* ---- Context：装配层唯一入口，保存 Legacy App 指针 ---- */
BMS_ServiceContext_t *BMS_ServiceInit(struct BQ76940_AppCtx *legacy);

/* ---- Sample ---- */
void    BMS_ServiceGetCalib(BMS_ServiceContext_t *svc, BQ76930_AdcCalib_t *calib);
uint8_t BMS_ServiceSampleReadHw(const BQ76930_AdcCalib_t *calib,
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
