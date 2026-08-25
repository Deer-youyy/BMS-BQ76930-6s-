#ifndef __BMS_SERVICE_H
#define __BMS_SERVICE_H

/**
 * Migration-005B: Task -> neutral BMS Service boundary header.
 *
 * ±¾Í·ÎÄ¼þ¶¨Òå Task ²ãÓë BMS Service Ö®¼äµÄÖÐÐÔ½Ó¿Ú£º
 * - ²»°üº¬ÈÎºÎ BQ76930 / BQ76940 App Í·ÎÄ¼þ£»
 * - ²»±©Â¶ BQ76940_AppCtx_t ÍêÕûÀàÐÍ£¨½öÇ°ÖÃÉùÃ÷£©£»
 * - ²»±©Â¶ÈÎºÎ BQ76930 ×¨ÊôÀàÐÍ£¨AdcCalib / CCRaw / HalFaultDecode ¾ùÒÑÖÐÐÔ»¯£©£»
 * - ²ÉÑùÊý¾ÝÊ¹ÓÃÖÐÐÔ BMS_SampleData_t / BMS_CellStats_t / BMS_CCRaw_t / BMS_AdcCalib_t£¬
 *   Service ÄÚ²¿Óë legacy BQ76940 / BQ76930 ÀàÐÍÖð×Ö¶ÎÓ³Éä
 *   £¨½ûÖ¹ÀàÐÍÇ¿×ª / sizeof / offsetof / union alias / memcpy£©¡£
 */

#include <stdint.h>
#include "../../Core/bms_types.h"

#define BMS_OT_ACTION_NONE         0U
#define BMS_OT_ACTION_FET_OFF      1U
#define BMS_OT_ACTION_FET_ON       2U

#define BMS_UT_ACTION_NONE         0U
#define BMS_UT_ACTION_CHG_OFF      1U
#define BMS_UT_ACTION_CHG_ON       2U

#define BMS_BAL_ACTION_NONE        0U
#define BMS_BAL_ACTION_START       1U
#define BMS_BAL_ACTION_STOP        2U

#define BMS_OCDSCD_ACTION_NONE      0U
#define BMS_OCDSCD_ACTION_DSG_OFF   1U
#define BMS_OCDSCD_ACTION_DSG_ON    2U

/* Legacy App Ç°ÖÃÉùÃ÷£¬±ÜÃâ±©Â¶ÍêÕû BQ76940_AppCtx_t¡£ */
struct BQ76940_AppCtx;

/* BMS Service ÉÏÏÂÎÄ¾ä±ú£¨²»Í¸Ã÷£¬Êµ¼Ê¶¨ÒåÎ»ÓÚ bms_service.c£©¡£ */
typedef struct BMS_ServiceContext BMS_ServiceContext_t;

/* ÖÐÐÔ ADC Ð£×¼£¨Öð×Ö¶ÎÓ³Éä×Ô ADC calibration£©¡£ */
typedef struct
{
    uint16_t gain_uV_per_lsb;
    int16_t  offset_mV;
} BMS_AdcCalib_t;

/* ÖÐÐÔ CC Ô­Ê¼Öµ£¨Öð×Ö¶ÎÓ³Éä×Ô CC raw£©¡£ */
typedef struct
{
    uint8_t  raw_hi;
    uint8_t  raw_lo;
    uint16_t raw_u16;
    int16_t  raw_s16;
} BMS_CCRaw_t;

/* ÖÐÐÔµ¼µç³ØÐ¾Í³¼Æ£¨Öð×Ö¶ÎÓ³Éä×Ô BQ76940_CellStats9_t£©¡£ */
typedef struct
{
    uint16_t max_mV;
    uint16_t min_mV;
    uint16_t diff_mV;
    uint8_t  max_cell_label;
    uint8_t  min_cell_label;
} BMS_CellStats_t;

/* ÖÐÐÔ²ÉÑùÊý¾Ý£¨Öð×Ö¶ÎÓ³Éä×Ô BQ76940_AppSampleData_t£©¡£ */
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

/* ÖÐÐÔÓ²¼þ¹ÊÕÏ×´Ì¬£¨Öð×Ö¶ÎÓ³Éä×Ô hardware fault decode£©¡£
 * current_fault_active == (ocd || scd)£»voltage_fault_active == (ov || uv)¡£ */
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
/* ä¸­æ€§ä¿æŠ¤è¯·æ±?DTOï¼ˆé€å­—æ®µæ˜ å°„è‡ª BQ76940_OtProtectRequest_tï¼‰ã€?*/
typedef struct
{
    uint8_t action;
    uint8_t ot_now;
    uint8_t ov_now;
    uint8_t uv_now;
    uint8_t ot_cutoff_active_snapshot;
} BMS_OtProtectRequest_t;

/* ä¸­æ€§æ¬ æ¸©ä¿æŠ¤è¯·æ±?DTOï¼ˆé€å­—æ®µæ˜ å°„è‡ª BQ76940_UtProtectRequest_tï¼‰ã€?*/
typedef struct
{
    uint8_t action;
    uint8_t ut_now;
    uint8_t ov_now;
    uint8_t ot_now;
    uint8_t ot_cutoff_active_snapshot;
    uint8_t ut_chg_block_active_snapshot;
} BMS_UtProtectRequest_t;

/* ä¸­æ€§å‡è¡¡é€»è¾‘ç”µæ±  maskï¼ˆbit i = é€»è¾‘ç”µæ±  iï¼›é€å­—æ®µæ˜ å°„è‡ª BqHalBalanceMask_tï¼‰ã€?*/
typedef struct
{
    uint32_t logical_mask;
} BMS_BalanceMask_t;

/* ä¸­æ€§å‡è¡¡è¯·æ±?DTOï¼ˆé€å­—æ®µæ˜ å°„è‡ª BQ76940_BalanceRequest_tï¼‰ã€?*/
typedef struct
{
    uint8_t action;
    uint8_t target_logical;
    uint8_t target_count;
    uint8_t reason;
    BMS_BalanceMask_t wr;
    BMS_BalanceMask_t rd;
} BMS_BalanceRequest_t;

/* ä¸­æ€?OCD/SCD è¯·æ±‚ DTOï¼ˆé€å­—æ®µæ˜ å°„è‡ª BQ76940_OcdScdRequest_tï¼‰ã€?*/
typedef struct
{
    uint8_t action;
    uint8_t sys_stat_snapshot;
    uint8_t hw_fault_now;
    uint8_t fault_code;
    uint8_t apply_ret;
    uint8_t ocd_now;
    uint8_t scd_now;
    uint8_t recover_request;
} BMS_OcdScdRequest_t;

/* ä¸­æ€§ç¡¬ä»¶æ•…éšœè®¡æ•°ï¼ˆä¾?Service Commit è¾“å‡ºï¼Œä¾› Task æ—¥å¿—ï¼‰ã€?*/
typedef struct
{
    uint8_t  hw_fault_last_code;
    uint8_t  sys_stat_latched;
    uint16_t hw_fault_count;
    uint8_t  last_apply_ret;
} BMS_HwFaultCounters_t;

/* ---- Context£º×°Åä²ãÎ¨Ò»Èë¿Ú£¬±£´æ Legacy App Ö¸Õë ---- */
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

/* ---- Hardware Fault£ºÖÐÐÔ SYS_STAT ½âÂë£¨´¿º¯Êý£¬ÎÞËø£© ---- */
void BMS_ServiceHwFaultDecode(uint8_t sys_stat, BMS_HwFaultState_t *fault_state);
/* ---- Protectï¼ˆé”åŸŸå†…è°ƒç”¨ï¼Œæ— é”ï¼‰ ---- */
void    BMS_ServiceRuntimeFaultActive(BMS_ServiceContext_t *svc, uint8_t *active);
uint8_t BMS_ServiceProtectCompute(BMS_ServiceContext_t *svc,
                                  BMS_OtProtectRequest_t *ot,
                                  BMS_UtProtectRequest_t *ut);
uint8_t BMS_ServiceProtectApplyI2c(BMS_OtProtectRequest_t *ot,
                                   BMS_UtProtectRequest_t *ut);
uint8_t BMS_ServiceProtectCommit(BMS_ServiceContext_t *svc,
                                 const BMS_OtProtectRequest_t *ot,
                                 const BMS_UtProtectRequest_t *ut);

/* ---- Balanceï¼ˆé”åŸŸå†…è°ƒç”¨ï¼Œæ— é”ï¼‰ ---- */
uint8_t BMS_ServiceBalanceDecide(BMS_ServiceContext_t *svc,
                                 BMS_BalanceRequest_t *req,
                                 uint32_t now_ms);
uint8_t BMS_ServiceBalanceApplyI2c(BMS_BalanceRequest_t *req);
uint8_t BMS_ServiceBalanceCommit(BMS_ServiceContext_t *svc,
                                 const BMS_BalanceRequest_t *req);

/* ---- Hardware Faultï¼ˆé”åŸŸå†…è°ƒç”¨ï¼Œæ— é”ï¼‰ ---- */
void    BMS_ServiceHwFaultRequestClear(BMS_OcdScdRequest_t *req);
uint8_t BMS_ServiceHwFaultDecide(BMS_ServiceContext_t *svc,
                                 BMS_OcdScdRequest_t *req,
                                 uint8_t sys_stat);
uint8_t BMS_ServiceHwFaultApplyI2c(BMS_OcdScdRequest_t *req);
uint8_t BMS_ServiceHwFaultCommit(BMS_ServiceContext_t *svc,
                                 const BMS_OcdScdRequest_t *req,
                                 BMS_HwFaultCounters_t *counters);

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
