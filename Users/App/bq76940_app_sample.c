#include "bq76940_app.h"
#include "bms_log.h"
#include "bq76940_app_sample.h"

#include "stdio.h"
#include "string.h"

/* 近零死区，避免 +1/-1 这种抖动误判
 * 当前先按 50mA 作为经验死区
 */
#define BQ76940_CURRENT_ZERO_DEADBAND_mA   50


static int8_t BQ76940_AppJudgeCurrentDir(int32_t current_mA)
{
    if (current_mA >= BQ76940_CURRENT_ZERO_DEADBAND_mA)
    {
        return 1;
    }
    else if (current_mA <= -BQ76940_CURRENT_ZERO_DEADBAND_mA)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

uint8_t BQ76940_AppSampleReadHw(const BQ76930_AdcCalib_t *calib,
                                BQ76940_AppSampleData_t *sample)
{
    uint8_t ret;
    BqHalMeasure_t *mea;
    BqHalAdcCalib_t ncalib;
    BqHalMeasureSample_t m;
    BqHalMeasureRawDiag_t raw;

    if ((calib == 0U) || (sample == 0U))
    {
        return 1U;
    }

    mea = BqHalMeasure_Get();
    if (mea == 0U)
    {
        /* 测量 Provider 未装配 */
        return 1U;
    }

    /*
     * 1. BQ76940_AdcCalib_t ? BqHalAdcCalib_t 无损映射（字段一一对应）。
     *    校准由 bring-up 单次读取后传入，不为本路径新增 I2C 访问。
     */
    ncalib.gain_uV_per_lsb = calib->gain_uV_per_lsb;
    ncalib.offset_mV       = calib->offset_mV;

    /*
     * 2. 通过中性测量接口完成一次硬件读取：
     *    Cell / CC / TS1 / SYS_STAT 仅读取一次，同时产出测量与诊断原始载荷。
     *    原 BQ76940 寄存器读取与换算已在 Provider 内完成。
     */
    ret = BqHalMeasure_ReadSample(mea, &ncalib, &m, &raw);
    if (ret != 0U)
    {
        /* 将中性错误码映射回原阶段错误码与日志，保持诊断语义一致 */
        switch (ret)
        {
            case BQHAL_MEASURE_ERR_CELL:
                return 11U;
            case BQHAL_MEASURE_ERR_CC_START:
                BMS_LOG_ERROR("[SMP] CC start:%d\r\n", ret);
                return 14U;
            case BQHAL_MEASURE_ERR_CC_WAIT:
                BMS_LOG_ERROR("[SMP] CC wait:%d\r\n", ret);
                return 15U;
            case BQHAL_MEASURE_ERR_CC_READ:
                BMS_LOG_ERROR("[SMP] CC read:%d\r\n", ret);
                return 16U;
            case BQHAL_MEASURE_ERR_TS1:
                BMS_LOG_ERROR("[SMP] TS1 read:%d\r\n", ret);
                return 18U;
            case BQHAL_MEASURE_ERR_SYS:
                BMS_LOG_ERROR("[SMP] SYS read:%d\r\n", ret);
                return 23U;
            case BQHAL_MEASURE_ERR_STATS:
                return 12U;
            case BQHAL_MEASURE_ERR_CC_CONV:
                BMS_LOG_ERROR("[SMP] CC conv:%d\r\n", ret);
                return 17U;
            case BQHAL_MEASURE_ERR_TS1_CONV:
                BMS_LOG_ERROR("[SMP] TS1 conv:%d\r\n", ret);
                return 19U;
            case BQHAL_MEASURE_ERR_STATUS:
                BMS_LOG_ERROR("[SMP] SYS mask:%d\r\n", ret);
                return 24U;
            default:
                return 1U;
        }
    }

    /*
     * 3. 中性测量 → BQ76940 采样快照映射。
     *    Print 所需的芯片专有原始字段由 RawDiag 回填，Print 保持不变。
     */
    memcpy(sample->cell_mV, m.cell_mV, sizeof(sample->cell_mV));
    memcpy(sample->cell_raw, raw.cell_raw, sizeof(sample->cell_raw));

    sample->pack_total_mV = m.pack_total_mV;

    sample->cell_stats.max_mV        = m.max_mV;
    sample->cell_stats.min_mV        = m.min_mV;
    sample->cell_stats.diff_mV       = m.diff_mV;
    sample->cell_stats.max_cell_label = m.max_label;
    sample->cell_stats.min_cell_label = m.min_label;

    sample->pack_current_mA  = m.pack_current_mA;
    sample->pack_current_dir = BQ76940_AppJudgeCurrentDir(m.pack_current_mA);

    sample->cc_raw.raw_hi = raw.cc_raw_hi;
    sample->cc_raw.raw_lo = raw.cc_raw_lo;
    sample->cc_raw.raw_u16 = (uint16_t)(((uint16_t)raw.cc_raw_hi << 8) |
                                        (uint16_t)raw.cc_raw_lo);
    sample->cc_raw.raw_s16 = raw.cc_raw_s16;

    sample->ts1_raw_adc = raw.ts1_raw_adc;
    sample->ts1_temp_dC = m.ts1_temp_dC;

    sample->sys_stat          = raw.sys_stat;
    sample->fault_mask_active = m.afe_status.fault_mask_active;

    return 0U;
}



uint8_t BQ76940_AppSampleProcess(BQ76940_AppSampleData_t *sample)
{
    if (sample == 0U)
    {
        return 1U;
    }

    /*
     * 采样读取与换算已统一下沉到 BqHalMeasure Provider，
     * 本函数保留为空实现以维持对外接口及 bms_tasks 调用关系不变。
     */
    return 0U;
}


uint8_t BQ76940_AppSampleCommit(BQ76940_AppCtx_t *ctx,
                                const BQ76940_AppSampleData_t *sample)
{
    if ((ctx == 0) || (sample == 0))
    {
        return 1U;
    }

    /*
     * 将局部采样快照提交到全局 app。
     *
     * 注意：
     *   该函数会修改 BQ76940_AppCtx_t，
     *   因此在 FreeRTOS 任务中调用时，应持有 g_bms_ctx_mutex。
     */
    memcpy(ctx->cell_raw,
           sample->cell_raw,
           sizeof(ctx->cell_raw));

    memcpy(ctx->cell_mV,
           sample->cell_mV,
           sizeof(ctx->cell_mV));

    ctx->pack_total_mV = sample->pack_total_mV;
    ctx->cell_stats    = sample->cell_stats;

    ctx->cc_raw           = sample->cc_raw;
    ctx->pack_current_mA  = sample->pack_current_mA;
    ctx->pack_current_dir = sample->pack_current_dir;

    ctx->ts1_raw_adc = sample->ts1_raw_adc;
    ctx->ts1_temp_dC = sample->ts1_temp_dC;

    ctx->sys_stat          = sample->sys_stat;
    ctx->fault_mask_active = sample->fault_mask_active;

    return 0U;
}


uint8_t BQ76940_AppSampleUpdate(BQ76940_AppCtx_t *ctx)
{
    uint8_t ret;
    BQ76940_AppSampleData_t sample;

    if (ctx == 0)
    {
        return 1U;
    }
    ret = BQ76940_AppSampleReadHw(&ctx->calib, &sample);
    if (ret != 0U)
    {
        return ret;
    }

    ret = BQ76940_AppSampleProcess(&sample);
    if (ret != 0U)
    {
        return ret;
    }

    ret = BQ76940_AppSampleCommit(ctx, &sample);
    if (ret != 0U)
    {
        return ret;
    }

    return 0U;
}


