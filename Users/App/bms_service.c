#include "bms_service.h"

/* Legacy App 类型仅在本实现文件中可见（逐字段映射所需）。 */
#include "bq76930_hal.h"
#include "bq76940_app.h"
#include "bq76940_app_sample.h"
#include "bq76940_app_runtime_diag.h"
#include "bq76940_app_control.h"
#include "bq76940_app_protect.h"
#include "bq76940_app_balance.h"
#include "bq76940_app_hw_fault.h"
#include "bq_hal_balance.h"
#include "bq76930_balance.h"

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


/* ---------------- Protect锛氫腑鎬ф槧灏?+ Service锛堥攣鍩熷唴璋冪敤锛屾棤閿侊級 ---------------- */

static void BMS_MapOtReqToLegacy(const BMS_OtProtectRequest_t *src,
                                 BQ76940_OtProtectRequest_t *dst)
{
    dst->action = src->action;
    dst->ot_now = src->ot_now;
    dst->ov_now = src->ov_now;
    dst->uv_now = src->uv_now;
    dst->ot_cutoff_active_snapshot = src->ot_cutoff_active_snapshot;
}

static void BMS_MapOtReqFromLegacy(const BQ76940_OtProtectRequest_t *src,
                                   BMS_OtProtectRequest_t *dst)
{
    dst->action = src->action;
    dst->ot_now = src->ot_now;
    dst->ov_now = src->ov_now;
    dst->uv_now = src->uv_now;
    dst->ot_cutoff_active_snapshot = src->ot_cutoff_active_snapshot;
}

static void BMS_MapUtReqToLegacy(const BMS_UtProtectRequest_t *src,
                                 BQ76940_UtProtectRequest_t *dst)
{
    dst->action = src->action;
    dst->ut_now = src->ut_now;
    dst->ov_now = src->ov_now;
    dst->ot_now = src->ot_now;
    dst->ot_cutoff_active_snapshot = src->ot_cutoff_active_snapshot;
    dst->ut_chg_block_active_snapshot = src->ut_chg_block_active_snapshot;
}

static void BMS_MapUtReqFromLegacy(const BQ76940_UtProtectRequest_t *src,
                                   BMS_UtProtectRequest_t *dst)
{
    dst->action = src->action;
    dst->ut_now = src->ut_now;
    dst->ov_now = src->ov_now;
    dst->ot_now = src->ot_now;
    dst->ot_cutoff_active_snapshot = src->ot_cutoff_active_snapshot;
    dst->ut_chg_block_active_snapshot = src->ut_chg_block_active_snapshot;
}

void BMS_ServiceRuntimeFaultActive(BMS_ServiceContext_t *svc, uint8_t *active)
{
    if (active == NULL)
    {
        return;
    }
    *active = 0U;

    if ((svc == NULL) || (svc->legacy == NULL))
    {
        return;
    }

    *active = (uint8_t)BQ76940_AppRuntimeDiagIsFaultActive(svc->legacy);
}

uint8_t BMS_ServiceProtectCompute(BMS_ServiceContext_t *svc,
                                  BMS_OtProtectRequest_t *ot,
                                  BMS_UtProtectRequest_t *ut)
{
    BQ76940_OtProtectRequest_t lot;
    BQ76940_UtProtectRequest_t lut;
    uint8_t ret;
    BQ76940_AppCtx_t *legacy;

    if ((svc == NULL) || (svc->legacy == NULL))
    {
        return 1U;
    }
    legacy = svc->legacy;

    ret = BQ76940_AppProtectUpdateBase(legacy);
    if (ret == 0U)
    {
        ret = BQ76940_AppOtProtectDecide(legacy, &lot);
    }
    if (ret == 0U)
    {
        ret = BQ76940_AppUtProtectDecide(legacy, &lut);
    }
    if (ret == 0U)
    {
        if (ot != NULL)
        {
            BMS_MapOtReqFromLegacy(&lot, ot);
        }
        if (ut != NULL)
        {
            BMS_MapUtReqFromLegacy(&lut, ut);
        }
    }

    return ret;
}

uint8_t BMS_ServiceProtectApplyI2c(BMS_OtProtectRequest_t *ot,
                                   BMS_UtProtectRequest_t *ut)
{
    BQ76940_OtProtectRequest_t lot;
    BQ76940_UtProtectRequest_t lut;
    uint8_t ret = 0U;

    if (ot == NULL)
    {
        return 1U;
    }
    if (ut == NULL)
    {
        return 1U;
    }

    BMS_MapOtReqToLegacy(ot, &lot);
    BMS_MapUtReqToLegacy(ut, &lut);

    if (lot.action != BQ76940_OT_ACTION_NONE)
    {
        ret = BQ76940_AppOtProtectApplyHw(&lot);
    }
    if ((ret == 0U) && (lut.action != BQ76940_UT_ACTION_NONE))
    {
        ret = BQ76940_AppUtProtectApplyHw(&lut);
    }

    return ret;
}

uint8_t BMS_ServiceProtectCommit(BMS_ServiceContext_t *svc,
                                 const BMS_OtProtectRequest_t *ot,
                                 const BMS_UtProtectRequest_t *ut)
{
    BQ76940_OtProtectRequest_t lot;
    BQ76940_UtProtectRequest_t lut;
    uint8_t ret;

    if ((svc == NULL) || (svc->legacy == NULL) || (ot == NULL) || (ut == NULL))
    {
        return 1U;
    }

    BMS_MapOtReqToLegacy(ot, &lot);
    BMS_MapUtReqToLegacy(ut, &lut);

    ret = BQ76940_AppOtProtectCommit(svc->legacy, &lot);
    if (ret == 0U)
    {
        ret = BQ76940_AppUtProtectCommit(svc->legacy, &lut);
    }

    return ret;
}

/* ---------------- Balance锛氫腑鎬ф槧灏?+ Service锛坆al_auto 鏇存柊鍦?Commit 鍐咃級 ---------------- */

static void BMS_MapBalanceMaskToLegacy(const BMS_BalanceMask_t *src,
                                       BqHalBalanceMask_t *dst)
{
    dst->logical_mask = src->logical_mask;
}

static void BMS_MapBalanceMaskFromLegacy(const BqHalBalanceMask_t *src,
                                         BMS_BalanceMask_t *dst)
{
    dst->logical_mask = src->logical_mask;
}

static void BMS_MapBalanceReqToLegacy(const BMS_BalanceRequest_t *src,
                                      BQ76940_BalanceRequest_t *dst)
{
    dst->action = src->action;
    dst->target_logical = src->target_logical;
    dst->target_count   = src->target_count;
    dst->reason         = src->reason;
    BMS_MapBalanceMaskToLegacy(&src->wr, &dst->wr);
    BMS_MapBalanceMaskToLegacy(&src->rd, &dst->rd);
}

static void BMS_MapBalanceReqFromLegacy(const BQ76940_BalanceRequest_t *src,
                                        BMS_BalanceRequest_t *dst)
{
    dst->action = src->action;
    dst->target_logical = src->target_logical;
    dst->target_count   = src->target_count;
    dst->reason         = src->reason;
    BMS_MapBalanceMaskFromLegacy(&src->wr, &dst->wr);
    BMS_MapBalanceMaskFromLegacy(&src->rd, &dst->rd);
}

uint8_t BMS_ServiceBalanceDecide(BMS_ServiceContext_t *svc,
                                 BMS_BalanceRequest_t *req,
                                 uint32_t now_ms)
{
    BQ76940_BalanceRequest_t lreq;
    uint8_t ret;

    if ((svc == NULL) || (svc->legacy == NULL) || (req == NULL))
    {
        return 1U;
    }

    BQ76940_AppBalanceRequestClear(&lreq);

    ret = BQ76940_AppBalanceDecide(svc->legacy, &lreq, now_ms);
    if (ret == 0U)
    {
        BMS_MapBalanceReqFromLegacy(&lreq, req);
    }

    return ret;
}

uint8_t BMS_ServiceBalanceApplyI2c(BMS_BalanceRequest_t *req)
{
    BQ76940_BalanceRequest_t lreq;
    uint8_t ret;

    if (req == NULL)
    {
        return 1U;
    }

    BMS_MapBalanceReqToLegacy(req, &lreq);

    ret = BQ76940_AppBalanceApplyHw(&lreq);
    if (ret == 0U)
    {
        BMS_MapBalanceReqFromLegacy(&lreq, req);
    }

    return ret;
}

uint8_t BMS_ServiceBalanceCommit(BMS_ServiceContext_t *svc,
                                 const BMS_BalanceRequest_t *req)
{
    BQ76940_BalanceRequest_t lreq;
    uint8_t ret;
    BQ76940_AppCtx_t *legacy;

    if ((svc == NULL) || (svc->legacy == NULL) || (req == NULL))
    {
        return 1U;
    }
    legacy = svc->legacy;

    BMS_MapBalanceReqToLegacy(req, &lreq);

    ret = BQ76940_AppBalanceCommit(legacy, &lreq);

    if ((ret == 0U) && (lreq.action != BQ76940_BAL_ACTION_NONE))
    {
        /* Legacy 鍙傝�冪姸鎬侊紙CAN 0x306 / Safe-Off锛夋敹杩?Service 鍐呴儴鏇存柊銆?*/
        BqHalBalance_GetLegacyCellBalRegs(&lreq.wr, &legacy->bal_auto_wr);
        BqHalBalance_GetLegacyCellBalRegs(&lreq.rd, &legacy->bal_auto_rd);
    }

    return ret;
}

/* ---------------- Hardware Fault锛氫腑鎬ф槧灏?+ Service ---------------- */

static void BMS_MapOcdReqToLegacy(const BMS_OcdScdRequest_t *src,
                                  BQ76940_OcdScdRequest_t *dst)
{
    dst->action = src->action;
    dst->sys_stat_snapshot = src->sys_stat_snapshot;
    dst->hw_fault_now = src->hw_fault_now;
    dst->fault_code   = src->fault_code;
    dst->apply_ret    = src->apply_ret;
    dst->ocd_now      = src->ocd_now;
    dst->scd_now      = src->scd_now;
    dst->recover_request = src->recover_request;
}

static void BMS_MapOcdReqFromLegacy(const BQ76940_OcdScdRequest_t *src,
                                    BMS_OcdScdRequest_t *dst)
{
    dst->action = src->action;
    dst->sys_stat_snapshot = src->sys_stat_snapshot;
    dst->hw_fault_now = src->hw_fault_now;
    dst->fault_code   = src->fault_code;
    dst->apply_ret    = src->apply_ret;
    dst->ocd_now      = src->ocd_now;
    dst->scd_now      = src->scd_now;
    dst->recover_request = src->recover_request;
}

void BMS_ServiceHwFaultRequestClear(BMS_OcdScdRequest_t *req)
{
    if (req == NULL)
    {
        return;
    }

    req->action           = BMS_OCDSCD_ACTION_NONE;
    req->sys_stat_snapshot = 0U;
    req->hw_fault_now      = 0U;
    req->fault_code        = 0U;
    req->apply_ret         = 0U;
    req->ocd_now           = 0U;
    req->scd_now           = 0U;
    req->recover_request   = 0U;
}

uint8_t BMS_ServiceHwFaultDecide(BMS_ServiceContext_t *svc,
                                 BMS_OcdScdRequest_t *req,
                                 uint8_t sys_stat)
{
    BQ76940_OcdScdRequest_t lreq;
    uint8_t ret;
    BQ76940_AppCtx_t *legacy;

    if ((svc == NULL) || (svc->legacy == NULL) || (req == NULL))
    {
        return 1U;
    }
    legacy = svc->legacy;

    legacy->sys_stat = sys_stat;

    BQ76940_AppOcdScdRequestClear(&lreq);

    ret = BQ76940_AppOcdScdDecide(legacy, &lreq);
    if (ret == 0U)
    {
        BMS_MapOcdReqFromLegacy(&lreq, req);
    }

    return ret;
}

uint8_t BMS_ServiceHwFaultApplyI2c(BMS_OcdScdRequest_t *req)
{
    BQ76940_OcdScdRequest_t lreq;

    if (req == NULL)
    {
        return 1U;
    }

    BMS_MapOcdReqToLegacy(req, &lreq);

    return BQ76940_AppOcdScdApplyHw(&lreq);
}

uint8_t BMS_ServiceHwFaultCommit(BMS_ServiceContext_t *svc,
                                 const BMS_OcdScdRequest_t *req,
                                 BMS_HwFaultCounters_t *counters)
{
    BQ76940_OcdScdRequest_t lreq;
    uint8_t ret;
    BQ76940_AppCtx_t *legacy;

    if ((svc == NULL) || (svc->legacy == NULL) || (req == NULL))
    {
        return 1U;
    }
    legacy = svc->legacy;

    BMS_MapOcdReqToLegacy(req, &lreq);

    ret = BQ76940_AppOcdScdCommit(legacy, &lreq);

    if (counters != NULL)
    {
        counters->hw_fault_last_code = legacy->hw_fault_last_code;
        counters->sys_stat_latched   = legacy->hw_fault_sys_stat_latched;
        counters->hw_fault_count     = legacy->hw_fault_count;
        counters->last_apply_ret     = legacy->hw_fault_last_apply_ret;
    }

    return ret;
}


uint8_t BMS_ServiceHwFaultReadSysStat(BMS_ServiceContext_t *svc, uint8_t *sys_stat)
{
    (void)svc;

    if (sys_stat == NULL)
    {
        return 1U;
    }

    return BQ76930_HalReadSysStat(sys_stat);
}
