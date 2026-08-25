#include "bms_service.h"

/* Legacy App 类型仅在本实现文件中可见（逐字段映射所需）。 */
#include "bq76930_hal.h"
#include "bq76940_app.h"
#include "bq76940_app_sample.h"
#include "bq76940_app_runtime_diag.h"
#include "bq76940_app_control.h"

/* BMS Service 上下文：不透明类型的实际定义。
 * 只保存装配层注入的 Legacy App 指针，所有 App 访问都走该指针。 */
struct BMS_ServiceContext
{
    BQ76940_AppCtx_t *legacy;
};

static BMS_ServiceContext_t g_service_ctx;

/* ---------------- 逐字段映射（禁止同布局强转 / sizeof / offsetof /
 *   union alias / memcpy 作为兼容性依据） ---------------- */

static void BMS_MapCellStatsToLegacy(const BMS_CellStats_t *src,
                                     BQ76940_CellStats9_t *dst)
{
    dst->max_mV = src->max_mV;
    dst->min_mV = src->min_mV;
    dst->diff_mV = src->diff_mV;
    dst->max_cell_label = src->max_cell_label;
    dst->min_cell_label = src->min_cell_label;
}

static void BMS_MapCellStatsFromLegacy(const BQ76940_CellStats9_t *src,
                                       BMS_CellStats_t *dst)
{
    dst->max_mV = src->max_mV;
    dst->min_mV = src->min_mV;
    dst->diff_mV = src->diff_mV;
    dst->max_cell_label = src->max_cell_label;
    dst->min_cell_label = src->min_cell_label;
}

static void BMS_MapSampleDataToLegacy(const BMS_SampleData_t *src,
                                      BQ76940_AppSampleData_t *dst)
{
    uint32_t i;

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        dst->cell_raw[i] = src->cell_raw[i];
        dst->cell_mV[i]  = src->cell_mV[i];
    }

    dst->pack_total_mV = src->pack_total_mV;
    BMS_MapCellStatsToLegacy(&src->cell_stats, &dst->cell_stats);

    dst->cc_raw.raw_hi  = src->cc_raw.raw_hi;
    dst->cc_raw.raw_lo  = src->cc_raw.raw_lo;
    dst->cc_raw.raw_u16 = src->cc_raw.raw_u16;
    dst->cc_raw.raw_s16 = src->cc_raw.raw_s16;

    dst->pack_current_mA  = src->pack_current_mA;
    dst->pack_current_dir = src->pack_current_dir;

    dst->ts1_raw_adc = src->ts1_raw_adc;
    dst->ts1_temp_dC = src->ts1_temp_dC;

    dst->sys_stat          = src->sys_stat;
    dst->fault_mask_active = src->fault_mask_active;
}

static void BMS_MapSampleDataFromLegacy(const BQ76940_AppSampleData_t *src,
                                        BMS_SampleData_t *dst)
{
    uint32_t i;

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        dst->cell_raw[i] = src->cell_raw[i];
        dst->cell_mV[i]  = src->cell_mV[i];
    }

    dst->pack_total_mV = src->pack_total_mV;
    BMS_MapCellStatsFromLegacy(&src->cell_stats, &dst->cell_stats);

    dst->cc_raw.raw_hi  = src->cc_raw.raw_hi;
    dst->cc_raw.raw_lo  = src->cc_raw.raw_lo;
    dst->cc_raw.raw_u16 = src->cc_raw.raw_u16;
    dst->cc_raw.raw_s16 = src->cc_raw.raw_s16;

    dst->pack_current_mA  = src->pack_current_mA;
    dst->pack_current_dir = src->pack_current_dir;

    dst->ts1_raw_adc = src->ts1_raw_adc;
    dst->ts1_temp_dC = src->ts1_temp_dC;

    dst->sys_stat          = src->sys_stat;
    dst->fault_mask_active = src->fault_mask_active;
}

/* ---------------- Context ---------------- */

BMS_ServiceContext_t *BMS_ServiceInit(BQ76940_AppCtx_t *legacy)
{
    g_service_ctx.legacy = legacy;
    return &g_service_ctx;
}

/* ---------------- Sample ---------------- */

void BMS_ServiceGetCalib(BMS_ServiceContext_t *svc,
                         BMS_AdcCalib_t *calib)
{
    if ((svc == NULL) || (svc->legacy == NULL) || (calib == NULL))
    {
        return;
    }

    calib->gain_uV_per_lsb = svc->legacy->calib.gain_uV_per_lsb;
    calib->offset_mV       = svc->legacy->calib.offset_mV;
}

uint8_t BMS_ServiceSampleReadHw(const BMS_AdcCalib_t *calib,
                                BMS_SampleData_t *sample)
{
    BQ76940_AppSampleData_t legacy_sample;
    BQ76930_AdcCalib_t legacy_calib;
    const BQ76930_AdcCalib_t *hw_calib = NULL;
    uint8_t ret;

    if (sample == NULL)
    {
        return 1U;
    }

    if (calib != NULL)
    {
        legacy_calib.gain_uV_per_lsb = calib->gain_uV_per_lsb;
        legacy_calib.offset_mV       = calib->offset_mV;
        hw_calib                     = &legacy_calib;
    }

    ret = BQ76940_AppSampleReadHw(hw_calib, &legacy_sample);
    if (ret != 0U)
    {
        return ret;
    }

    BMS_MapSampleDataFromLegacy(&legacy_sample, sample);

    return 0U;
}

uint8_t BMS_ServiceSampleProcess(BMS_SampleData_t *sample)
{
    BQ76940_AppSampleData_t legacy_sample;
    uint8_t ret;

    if (sample == NULL)
    {
        return 1U;
    }

    BMS_MapSampleDataToLegacy(sample, &legacy_sample);

    ret = BQ76940_AppSampleProcess(&legacy_sample);
    if (ret != 0U)
    {
        return ret;
    }

    BMS_MapSampleDataFromLegacy(&legacy_sample, sample);

    return 0U;
}

uint8_t BMS_ServiceSampleCommit(BMS_ServiceContext_t *svc,
                                const BMS_SampleData_t *sample,
                                uint8_t *notify_protect)
{
    BQ76940_AppSampleData_t legacy_sample;
    BQ76940_AppCtx_t *legacy;
    uint8_t recovered = 0U;
    uint8_t ret;

    if ((svc == NULL) || (svc->legacy == NULL) || (sample == NULL))
    {
        if (notify_protect != NULL)
        {
            *notify_protect = 0U;
        }
        return 1U;
    }

    legacy = svc->legacy;

    if (notify_protect != NULL)
    {
        *notify_protect = 0U;
    }

    BMS_MapSampleDataToLegacy(sample, &legacy_sample);

    /* SampleCommit = Commit + RuntimeDiag 成功记录 + 故障活跃查询。 */
    ret = BQ76940_AppSampleCommit(legacy, &legacy_sample);
    if (ret != 0U)
    {
        return ret;
    }

    BQ76940_AppRuntimeDiagRecordSampleOk(legacy, &recovered);

    if (BQ76940_AppRuntimeDiagIsFaultActive(legacy) == 0U)
    {
        if (notify_protect != NULL)
        {
            *notify_protect = 1U;
        }
    }

    return 0U;
}

void BMS_ServiceSampleReportFail(BMS_ServiceContext_t *svc,
                                 uint8_t fault_code,
                                 uint8_t fault_stage,
                                 uint8_t ret,
                                 uint8_t *enter_fault)
{
    if ((svc == NULL) || (svc->legacy == NULL))
    {
        if (enter_fault != NULL)
        {
            *enter_fault = 0U;
        }
        return;
    }

    if (enter_fault != NULL)
    {
        *enter_fault = 0U;
    }

    BQ76940_AppRuntimeDiagRecordSampleFail(svc->legacy,
                                           fault_code,
                                           fault_stage,
                                           ret,
                                           enter_fault);
}

/* ---------------- Hardware Fault：中性 SYS_STAT 解码（纯函数，无锁） ---------------- */

void BMS_ServiceHwFaultDecode(uint8_t sys_stat, BMS_HwFaultState_t *fault_state)
{
    BQ76930_HalFaultDecode_t dec;

    if (fault_state == NULL)
    {
        return;
    }

    BQ76930_HalDecodeSysStat(sys_stat, &dec);

    fault_state->ocd                  = dec.ocd;
    fault_state->scd                  = dec.scd;
    fault_state->ov                   = dec.ov;
    fault_state->uv                   = dec.uv;
    fault_state->device_not_ready     = dec.device_xready;
    fault_state->override_alert       = dec.ovrd_alert;
    fault_state->cc_ready             = dec.cc_ready;
    fault_state->current_fault_active = (dec.current_fault_mask != 0U) ? 1U : 0U;
    fault_state->voltage_fault_active = (dec.voltage_fault_mask != 0U) ? 1U : 0U;
}

/* ---------------- Runtime ---------------- */

void BMS_ServiceRuntimeTakeSafeOff(BMS_ServiceContext_t *svc,
                                   uint8_t *need_safe_off)
{
    if ((svc == NULL) || (svc->legacy == NULL))
    {
        if (need_safe_off != NULL)
        {
            *need_safe_off = 0U;
        }
        return;
    }

    BQ76940_AppRuntimeDiagTakeSafeOffRequest(svc->legacy, need_safe_off);
}

void BMS_ServiceRuntimeForceExternalOff(BMS_ServiceContext_t *svc)
{
    if ((svc == NULL) || (svc->legacy == NULL))
    {
        return;
    }

    (void)BQ76940_AppForceExternalOff(svc->legacy);
}

uint8_t BMS_ServiceRuntimeAfeOffHw(void)
{
    return BQ76940_AppForceAfeOffHw();
}

void BMS_ServiceRuntimeAfeOffCommit(BMS_ServiceContext_t *svc,
                                    uint8_t safe_off_result,
                                    uint8_t *retry_allowed)
{
    BQ76940_AppCtx_t *legacy;

    if ((svc == NULL) || (svc->legacy == NULL))
    {
        if (retry_allowed != NULL)
        {
            *retry_allowed = 0U;
        }
        return;
    }

    legacy = svc->legacy;

    /* RuntimeAfeOffCommit = 硬件回写提交 + RuntimeDiag 安全关断结果记录。 */
    BQ76940_AppForceAfeOffCommit(legacy, safe_off_result);

    BQ76940_AppRuntimeDiagCommitSafeOffResult(legacy,
                                              safe_off_result,
                                              retry_allowed);
}

/* ---------------- Control ---------------- */

uint8_t BMS_ServiceControlUpdate(BMS_ServiceContext_t *svc)
{
    if ((svc == NULL) || (svc->legacy == NULL))
    {
        return 1U;
    }

    return BQ76940_AppControlUpdate(svc->legacy);
}
