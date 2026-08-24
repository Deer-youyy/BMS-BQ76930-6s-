/* ****************************************************
 * BQ76940 测量 Provider（当前硬件支撑实现）
 *
 * 位置：bq_hal/ 目录，实现芯片无关的 BqHalMeasure 接口。
 * 作用：把 BqHalMeasure 中性测量/状态接口映射到 BQ76940 驱动。
 *
 * 说明：
 *   本 Provider 为“当前硬件（BQ76940）支撑”的临时适配实现，
 *   用于把业务采样代码与 BQ76940 数据读取解耦；后续 BQ76930 / 6S
 *   上线时可被替换为对应 Provider，而不影响上层语义接口。
 *   本文件只做测量数据访问，不包含任何 BMS 业务策略。
 *
 * 架构：
 *   App / Component
 *        |
 *   BqHalMeasure（中性测量接口）
 *        |
 *   bq76940_measure.c（当前 Provider）
 *        |
 *   BQ76940 Driver
 *        |
 *     Hardware
 * **************************************************** */

#include "bq_hal_measure.h"
#include "bq76940_drv.h"
#include "bq76940_protect.h"

/* 采样电阻值，单位 uOhm；必须与 App 层 BQ76940_RSENSE_UOHM(4000U) 保持一致，
 * 否则电流换算结果会发生偏移。此常量不允许单独改动。 */
#define BQ76940_RSENSE_UOHM   4000U

/* CC 1-shot 等待超时，单位 ms；与现状保持一致 */
#define BQ76940_PROBE_CC_TIMEOUT_MS 600U

/* 不透明句柄内部结构：仅作为绑定占位，当前无需额外字段 */
struct BqHalMeasure
{
    uint32_t model_ver; /* Provider 版本标识，预留给未来区分具体芯片 */
};

static struct BqHalMeasure s_bq76940_measure;
static struct BqHalMeasure *s_default_measure = 0U;

/* Provider 内部构造函数：绑定当前（BQ76940 支撑）测量 Provider。
 * 该函数为内部实现，main 只调用中性名 BqHalMeasure_LoadDefault()，
 * 不把“业务层选择 BQ76940”的语义暴露给上层。 */
static BqHalMeasure_t *BQ76940_MeasureDefault(void)
{
    s_default_measure = &s_bq76940_measure;
    return &s_bq76940_measure;
}

void BqHalMeasure_LoadDefault(void)
{
    (void)BQ76940_MeasureDefault();
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
    BQ76940_AdcCalib_t b_calib;
    BQ76940_CellStats9_t stats;
    BQ76940_CCRaw_t cc;
    uint16_t ts1_raw;
    uint16_t cell_raw[BMS_CELL_COUNT];
    uint16_t *cell_raw_out;

    if ((mea == 0U) || (calib == 0U) || (m == 0U))
    {
        return BQHAL_MEASURE_ERR_CELL;
    }

    /* BQ76940_AdcCalib_t ? BqHalAdcCalib_t 无损映射（字段一一对应） */
    b_calib.gain_uV_per_lsb = calib->gain_uV_per_lsb;
    b_calib.offset_mV       = calib->offset_mV;

    /*
     * 1. 单体电压：一次读取同时得到 raw + mV，不额外访问硬件。
     *    该函数内部通过 I2C 访问 BQ76940，调用前上层应已取得 I2C 总线互斥锁。
     */
    cell_raw_out = (raw != 0U) ? raw->cell_raw : cell_raw;
    ret = BQ76940_ReadAllMappedCellVoltages9_mV(&b_calib,
                                                cell_raw_out,
                                                m->cell_mV);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CELL;
    }

    /* 2. 包总压（只依赖已读取的 cell_mV，不访问 I2C） */
    m->pack_total_mV = BQ76940_CalcPackVoltage9_mV(m->cell_mV);

    /* 3. 单体统计：最高 / 最低 / 压差 / 对应逻辑编号 */
    ret = BQ76940_AnalyzeCellVoltages9(m->cell_mV, &stats);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_STATS;
    }
    m->min_mV    = stats.min_mV;
    m->max_mV    = stats.max_mV;
    m->diff_mV   = stats.diff_mV;
    m->min_label = stats.min_cell_label;
    m->max_label = stats.max_cell_label;

    /* 4. CC 电流：触发 1-shot → 等待 ready → 读取 → 换算 mA */
    ret = BQ76940_CC_StartOneShot();
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CC_START;
    }

    ret = BQ76940_CC_WaitReady(BQ76940_PROBE_CC_TIMEOUT_MS);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CC_WAIT;
    }

    ret = BQ76940_CC_ReadRaw(&cc);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_CC_READ;
    }

    ret = BQ76940_CC_ConvertToCurrent_mA(cc.raw_s16,
                                         BQ76940_RSENSE_UOHM,
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

    /* 5. TS1 温度：读取原始 ADC → 换算 dC */
    ret = BQ76940_ReadTS1Raw(&ts1_raw);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_TS1;
    }

    ret = BQ76940_ConvertTS1Temp_dC(ts1_raw, &m->ts1_temp_dC);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_TS1_CONV;
    }
    if (raw != 0U)
    {
        raw->ts1_raw_adc = ts1_raw;
    }

    /* 6. AFE 状态：读取 SYS_STAT → 提取当前激活故障掩码（中性语义） */
    ret = BQ76940_ProtectReadFaultStatus(&sys_stat);
    if (ret != 0U)
    {
        return BQHAL_MEASURE_ERR_SYS;
    }

    ret = BQ76940_ProtectGetActiveFaultMask(sys_stat,
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