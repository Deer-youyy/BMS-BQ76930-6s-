/* BQ76930 Measurement Provider (real 6S hardware).
 *
 * Implements the chip-agnostic BqHalMeasure interface on top of the
 * BQ76930 Driver:
 *   - cell voltages: logical cell 0..5 -> VC1/VC2/VC5/VC6/VC7/VC10
 *   - pack voltage  : sum of 6 logical cells
 *   - current       : continuous CC (CC_EN=1), 4 mOhm RSENSE -> raw * 2.11 mA
 *   - temperature   : TS2 (SYS_CTRL1 = 0x18, TEMP_SEL=1)
 *   - AFE status    : SYS_STAT -> neutral fault mask
 *
 * App layer only sees the neutral BqHalMeasureSample_t / RawDiag_t.
 */

#include "bq_hal_measure.h"
#include "bq76930_drv.h"

/* 4 mOhm sense resistor; 8440/4000 = 2.11 mA per CC LSB. */
#define BQ76930_RSENSE_UOHM   4000U

/* Provider internal state (opaque to upper layer). */
struct BqHalMeasure
{
    uint32_t model_ver;
};

static struct BqHalMeasure s_bq76930_measure;
static struct BqHalMeasure *s_default_measure = 0U;

static BqHalMeasure_t *BQ76930_MeasureDefault(void)
{
    s_default_measure = &s_bq76930_measure;
    return &s_bq76930_measure;
}

void BqHalMeasure_LoadDefault(void)
{
    (void)BQ76930_MeasureDefault();
}

BqHalMeasure_t *BqHalMeasure_Get(void)
{
    return s_default_measure;
}

uint8_t BqHalMeasure_ReadSample(BqHalMeasure_t *mea,
                                const BqHalAdcCalib_t *calib,
                                BqHalMeasureSample_t *m,
                                BqHalMeasureRawDiag_t *raw)
{
    uint8_t ret;
    uint8_t sys_stat;
    BQ76930_AdcCalib_t b_calib;
    BQ76930_CellStats_t stats;
    BQ76930_CCRaw_t cc;
    uint16_t ts2_raw;
    uint16_t cell_raw[BMS_CELL_COUNT];
    uint16_t *cell_raw_out;

    if ((mea == 0U) || (calib == 0U) || (m == 0U))
    {
        return BQHAL_MEASURE_ERR_CELL;
    }

    /* BQ76930_AdcCalib_t <- BqHalAdcCalib_t (same fields). */
    b_calib.gain_uV_per_lsb = calib->gain_uV_per_lsb;
    b_calib.offset_mV       = calib->offset_mV;

    /* 1. Cell voltages (raw + mV in one pass). */
    cell_raw_out = (raw != 0U) ? raw->cell_raw : cell_raw;
    ret = BQ76930_ReadAllCellVoltages_mV(&b_calib, cell_raw_out, m->cell_mV);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CELL;
    }

    /* 2. Pack voltage (no extra I2C). */
    m->pack_total_mV = BQ76930_CalcPackVoltage_mV(m->cell_mV);

    /* 3. Cell statistics (labels are real VC labels 1/2/5/6/7/10). */
    ret = BQ76930_AnalyzeCellVoltages(m->cell_mV, &stats);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_STATS;
    }
    m->min_mV    = stats.min_mV;
    m->max_mV    = stats.max_mV;
    m->diff_mV   = stats.diff_mV;
    m->min_label = stats.min_cell_label;
    m->max_label = stats.max_cell_label;

    /* 4. Current: continuous CC mode (CC_EN set by init), read + convert.
     *    No 1-shot + wait CC_READY sequence (BQ76940 behavior). */
    ret = BQ76930_ReadCCRaw(&cc);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CC_READ;
    }

    ret = BQ76930_ConvertCurrent_mA(cc.raw_s16,
                                    BQ76930_RSENSE_UOHM,
                                    &m->pack_current_mA);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CC_CONV;
    }
    if (raw != 0U)
    {
        raw->cc_raw_hi  = cc.raw_hi;
        raw->cc_raw_lo  = cc.raw_lo;
        raw->cc_raw_s16 = cc.raw_s16;
    }

    /* 5. Temperature: real hardware uses TS2. */
    ret = BQ76930_ReadNtcRaw(BQ76930_REG_TS2_HI, BQ76930_REG_TS2_LO, &ts2_raw);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_TS1;
    }

    ret = BQ76930_ConvertNtcTemp_dC(ts2_raw, &m->ts1_temp_dC);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_TS1_CONV;
    }
    if (raw != 0U)
    {
        raw->ts1_raw_adc = ts2_raw;
    }

    /* 6. AFE status: SYS_STAT -> neutral fault mask. */
    ret = BQ76930_ReadSysStat(&sys_stat);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_SYS;
    }

    ret = BQ76930_ProtectGetActiveFaultMask(sys_stat,
                                            &m->afe_status.fault_mask_active);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_STATUS;
    }
    if (raw != 0U)
    {
        raw->sys_stat = sys_stat;
    }

    return BQHAL_MEASURE_OK;
}
