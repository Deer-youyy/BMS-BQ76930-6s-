#ifndef __BQ76930_HAL_H
#define __BQ76930_HAL_H

/* BQ76930 Hardware Adapter / Provider layer interface.
 *
 * App layer calls these functions for FET / protection / bring-up /
 * SYS_STAT access. All BQ76930 register and bit-mask details stay
 * inside this Provider layer and the BQ76930 Driver.
 */

#include "bq_hal.h"
#include "bq76930_drv.h"

/* ---- FET control ---- */
uint8_t BQ76930_HalApplyFetEn(uint8_t chg_en, uint8_t dsg_en);
uint8_t BQ76930_HalSetCHG(uint8_t chg_on);
uint8_t BQ76930_HalSetDSG(uint8_t dsg_on);

/* ---- SYS_STAT / SYS_CTRL2 read ---- */
uint8_t BQ76930_HalReadSysStat(uint8_t *sys_stat);
uint8_t BQ76930_HalReadSysCtrl2(uint8_t *sys_ctrl2);

/* Neutral fault decode of SYS_STAT (bit positions stay in Provider). */
typedef struct
{
    uint8_t ocd;
    uint8_t scd;
    uint8_t ov;
    uint8_t uv;
    uint8_t device_xready;
    uint8_t ovrd_alert;
    uint8_t cc_ready;
    uint8_t hw_latch_mask;
    uint8_t current_fault_mask;
    uint8_t voltage_fault_mask;
} BQ76930_HalFaultDecode_t;

void BQ76930_HalDecodeSysStat(uint8_t sys_stat, BQ76930_HalFaultDecode_t *dec);

/* Return the latched hardware-fault bits of SYS_STAT (neutral mask). */
uint8_t BQ76930_HalFaultLatchMask(uint8_t sys_stat);

/* ---- bring-up / init (real 6S product behavior, BQ_1_config) ---- */
uint8_t BQ76930_HalInit(void);
uint8_t BQ76930_HalReadBasicRegs(BQ76930_BasicRegs_t *regs);
uint8_t BQ76930_HalGetAdcCalib(BQ76930_AdcCalib_t *calib);
uint8_t BQ76930_HalLoadProtection(uint8_t protect3,
                                  uint16_t ov_target_mV,
                                  uint16_t uv_target_mV,
                                  const BQ76930_AdcCalib_t *calib);

/* ---- balance raw registers ---- */
uint8_t BQ76930_HalReadCellBalRegs(BQ76930_CellBalRegs_t *regs);

#endif /* __BQ76930_HAL_H */
