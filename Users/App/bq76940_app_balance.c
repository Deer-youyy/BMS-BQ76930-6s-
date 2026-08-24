#include "bq76940_app.h"
#include "bms_log.h"
#include "bq76940_app_balance.h"
#include "bq_hal_balance.h"

#include "stdio.h"

/* Balance App 只保存上一次已确认的业务均衡 mask。
 * 该 mask 在 Write + Read Verify 成功后由 Balance 更新，
 * 不直接接触 BQ76940 寄存器细节。 */
static BqHalBalanceMask_t s_last_confirmed_mask;
static uint8_t s_last_confirmed_valid;

static uint8_t BQ76940_AppBalanceIsAdjacentIndex(uint8_t a, uint8_t b)
{
    if (a > b)
    {
        return ((uint8_t)(a - b) == 1U) ? 1U : 0U;
    }

    return ((uint8_t)(b - a) == 1U) ? 1U : 0U;
}

static uint8_t BQ76940_AppBalanceRefreshDue(uint32_t now_ms,
                                            uint32_t last_ms,
                                            uint32_t period_ms)
{
    if (period_ms == 0U)
    {
        return 1U;
    }

    if ((uint32_t)(now_ms - last_ms) >= period_ms)
    {
        return 1U;
    }

    return 0U;
}

static uint8_t BQ76940_AppBuildMultiCellBalMask(const BQ76940_AppCtx_t *ctx,
                                                BQ76940_BalanceRequest_t *req,
                                                uint16_t select_diff_mV,
                                                uint8_t use_parity,
                                                uint8_t parity_phase)
{
    uint8_t selected_idx[BQ76940_BAL_MAX_TARGET_COUNT];
    uint8_t selected_count = 0U;
    uint8_t used[BMS_CELL_COUNT];
    uint8_t i;
    uint8_t j;
    uint8_t best_idx;
    uint16_t best_mV;
    uint8_t found;
    uint8_t adjacent;
    uint16_t candidate_threshold_mV;

    BqHalBalanceMask_t one_mask;

    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        used[i] = 0U;
    }

    BqHalBalanceMask_Clear(&req->wr);

    /*
     * 计算候选阈值：
     * 候选条件为 单体电压 >= 最低单体电压 + diff_enter_mV
     *
     * 选择最高电压的多个非相邻单体，同时受奇偶窗口约束。
     */
    candidate_threshold_mV = ctx->cell_stats.min_mV + select_diff_mV;

    while (selected_count < BQ76940_BAL_MAX_TARGET_COUNT)
    {
        found = 0U;
        best_idx = 0U;
        best_mV = 0U;

        /*
         * 从所有未选中的逻辑单体中挑选候选。
         */
        for (i = 0U; i < BMS_CELL_COUNT; i++)
        {
            if (used[i] != 0U)
            {
                continue;
            }

            /*
             * 奇偶窗口筛选：
             * 当前只考虑逻辑下标 i 的奇偶，而不是 VC label 的奇偶。
             */
            if (use_parity != 0U)
            {
                if ((uint8_t)(i & 0x01U) != (uint8_t)(parity_phase & 0x01U))
                {
                    continue;
                }
            }

            if (ctx->cell_mV[i] < candidate_threshold_mV)
            {
                continue;
            }

            if (ctx->cell_mV[i] < ctx->bal_cfg.min_cell_mV)
            {
                continue;
            }

            /*
             * 不允许和已经选中的电芯相邻。
             */
            adjacent = 0U;
            for (j = 0U; j < selected_count; j++)
            {
                if (BQ76940_AppBalanceIsAdjacentIndex(i, selected_idx[j]) != 0U)
                {
                    adjacent = 1U;
                    break;
                }
            }

            if (adjacent != 0U)
            {
                continue;
            }

            if ((found == 0U) || (ctx->cell_mV[i] > best_mV))
            {
                found = 1U;
                best_idx = i;
                best_mV = ctx->cell_mV[i];
            }
        }

        if (found == 0U)
        {
            break;
        }

        /*
         * 选中 best_idx，对应一个真实 BQ76940 cell label。
         * 通过中性 mask 逐位累加到本轮 wr mask 上。
         */
        selected_idx[selected_count] = best_idx;
        selected_count++;

        used[best_idx] = 1U;

        BqHalBalanceMask_Clear(&one_mask);

        if (BqHalBalanceMask_SetLogical(&one_mask, best_idx) != BQHAL_BALANCE_OK)
        {
            return 2U;
        }

        BqHalBalanceMask_Or(&req->wr, &one_mask);
    }

    req->target_count = selected_count;

    if (selected_count != 0U)
    {
        /*
         * target_logical 仍然保存最高目标逻辑下标，方便日志和 CAN 兼容旧字段。
         * 最终由 Provider 转换成 VC label。
         */
        req->target_logical = selected_idx[0];
    }

    return 0U;
}

static uint8_t BQ76940_AppIsBalanceAllowed(const BQ76940_AppCtx_t *ctx)
{
    int32_t abs_current_mA;

    if (ctx == 0)
    {
        return 0;
    }

    abs_current_mA = (ctx->pack_current_mA >= 0) ? ctx->pack_current_mA : (-ctx->pack_current_mA);

    /* 保护与告警条件下禁止均衡 */
    if (ctx->alarm_state.uv_flag != 0U)
        return 0;
    if (ctx->alarm_state.ov_flag != 0U)
        return 0;
    if (ctx->alarm_state.ot_flag != 0U)
        return 0;
    if (ctx->alarm_state.ut_flag != 0U)
        return 0;

    if (ctx->ot_cutoff_active != 0U)
        return 0;
    if (ctx->ut_chg_block_active != 0U)
        return 0;
    if (ctx->hw_dsg_block_active != 0U)
        return 0;
    if (ctx->hw_ocd_active != 0U)
        return 0;
    if (ctx->hw_scd_active != 0U)
        return 0;

    if (ctx->runtime_diag.fault_active != 0U)
        return 0;

    /* 电流太大时先不均衡 */
    if (abs_current_mA > ctx->bal_cfg.max_abs_current_mA)
        return 0;

    /* 最高单体电压太低时先不均衡 */
    if (ctx->cell_stats.max_mV < ctx->bal_cfg.min_cell_mV)
        return 0;

    return 1;
}

void BQ76940_AppBalanceRequestClear(BQ76940_BalanceRequest_t *req)
{
    if (req == 0)
    {
        return;
    }

    req->action = BQ76940_BAL_ACTION_NONE;
    req->target_logical = 0U;
    req->target_count = 0U;
    req->reason = BQ76940_BAL_REASON_NONE;

    BqHalBalanceMask_Clear(&req->wr);
    BqHalBalanceMask_Clear(&req->rd);
}

uint8_t BQ76940_AppBalanceDecide(BQ76940_AppCtx_t *ctx, BQ76940_BalanceRequest_t *req, uint32_t now_ms)
{
    uint8_t allow_balance;
    uint8_t ret;

    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    BQ76940_AppBalanceRequestClear(req);

    /*
     * Decide 阶段只做判断：
     *   - 读取当前 app 状态
     *   - 判断是否允许均衡
     *   - 判断是否需要 START / STOP
     *   - 生成准备写入的中性均衡 mask
     *
     * 注意：
     *   这里不访问 I2C，不写 BQ76940。
     */
    allow_balance = BQ76940_AppIsBalanceAllowed(ctx);

    /*
     * 情况 1：
     * 当前不允许均衡，但之前处于均衡状态。
     * 需要关闭所有 CELLBAL。
     */
    if (allow_balance == 0U)
    {
        if (ctx->bal_active != 0U)
        {
            req->action = BQ76940_BAL_ACTION_STOP;
            req->reason = BQ76940_BAL_REASON_NOT_ALLOWED;

            BqHalBalanceMask_Clear(&req->wr);
        }

        return 0U;
    }

    /*
     * 情况 2：
     * 当前允许均衡，且还没有处于均衡状态。
     * 如果压差达到进入阈值，则对最高单体开启均衡。
     */

    if (ctx->bal_active == 0U)
    {
        if (ctx->cell_stats.diff_mV >= ctx->bal_cfg.diff_enter_mV)
        {
            /*
             * 未均衡启动时：
             * 使用全局非相邻贪心，不使用奇偶窗口。
             * 这样可以避免刚好当前奇偶窗口没有候选，导致明明需要均衡却无法启动。
             */
            ret = BQ76940_AppBuildMultiCellBalMask(ctx, req, ctx->bal_cfg.diff_enter_mV, 0U, 0U);
            if (ret != 0U)
            {
                return 2U;
            }

            if (req->target_count != 0U)
            {
                req->action = BQ76940_BAL_ACTION_START;
                ctx->bal_last_refresh_ms = now_ms;
            }
        }

        return 0U;
    }

    /*
     * 情况 3：
     * 当前已经处于均衡状态。
     * 如果压差降到退出阈值，则关闭均衡。
     */
    if (ctx->cell_stats.diff_mV <= ctx->bal_cfg.diff_exit_mV)
    {
        req->action = BQ76940_BAL_ACTION_STOP;
        req->reason = BQ76940_BAL_REASON_DIFF_EXIT;

        BqHalBalanceMask_Clear(&req->wr);

        return 0U;
    }

    /*
     * 情况 4：
     * 当前已经处于均衡状态，且没有达到退出条件。
     *
     * Balance V2.1：
     *   - 使用系统 tick 控制刷新周期，不依赖任务执行次数。
     *   - exit < diff < enter 时处于滞回保持区间，只保持当前 mask，不重算。
     *   - diff >= enter 且刷新周期到期时，切换奇偶窗口并重新计算 mask。
     *   - 新 mask 与当前硬件读回 mask 不一致时，才重新写 CELLBAL。
     */

    /* 刷新周期未到：不重算，不写 I2C */
    if (BQ76940_AppBalanceRefreshDue(now_ms,
                                     ctx->bal_last_refresh_ms,
                                     ctx->bal_cfg.refresh_period_ms) == 0U)
    {
        return 0U;
    }

    uint16_t select_diff_mV;

    /*
     * 刷新周期到期，更新时间戳。
     */
    ctx->bal_last_refresh_ms = now_ms;

    /*
     * 只要还没有低到 exit，就允许周期刷新。
     * diff >= enter：强均衡区，用 enter 作为候选阈值
     * exit < diff < enter：滞回保持区，用 exit 作为候选阈值
     */
    if (ctx->cell_stats.diff_mV >= ctx->bal_cfg.diff_enter_mV)
    {
        select_diff_mV = ctx->bal_cfg.diff_enter_mV;
    }
    else
    {
        select_diff_mV = ctx->bal_cfg.diff_exit_mV;
    }

    /*
     * 只要 Tick 到期并且仍处于均衡保持区，就翻转奇偶窗口。
     */
    if (ctx->bal_cfg.parity_enable != 0U)
    {
        ctx->bal_parity_phase ^= 1U;
    }

    /*
     * 优先使用奇偶窗口 + 非相邻贪心。
     */
    ret = BQ76940_AppBuildMultiCellBalMask(ctx,
                                           req,
                                           select_diff_mV,
                                           ctx->bal_cfg.parity_enable,
                                           ctx->bal_parity_phase);

    BMS_LOG_BALANCE("[BAL] Refresh phase:%d diff:%u old:%08X new:%08X count:%d\r\n",
                    ctx->bal_parity_phase,
                    ctx->cell_stats.diff_mV,
                    s_last_confirmed_mask.logical_mask,
                    req->wr.logical_mask,
                    req->target_count);

    if (ret != 0U)
    {
        return 2U;
    }

    /*
     * 如果当前奇偶窗口没有选出目标，则回退到全局非相邻贪心。
     * 防止因为窗口筛选太严格，导致明明有高压候选却不更新。
     */
    if ((req->target_count == 0U) && (ctx->bal_cfg.parity_enable != 0U))
    {
        ret = BQ76940_AppBuildMultiCellBalMask(ctx, req, select_diff_mV, 0U, 0U);
        if (ret != 0U)
        {
            return 2U;
        }
    }

    if (req->target_count == 0U)
    {
        return 0U;
    }

    /*
     * 新 mask 与上一次已确认 mask 不一致时，才更新 CELLBAL。
     */
    if ((s_last_confirmed_valid != 0U) &&
        (BqHalBalanceMask_Equal(&req->wr, &s_last_confirmed_mask) != 0U))
    {
        /* mask 未变化：不写 */
    }
    else
    {
        req->action = BQ76940_BAL_ACTION_START;
    }

    return 0U;
}

uint8_t BQ76940_AppBalanceApplyHw(BQ76940_BalanceRequest_t *req)
{
    BqHalBalance_t *bal;
    uint8_t ret;

    if (req == 0)
    {
        return 1U;
    }

    /*
     * 没有动作时，不访问 I2C。
     */
    if (req->action == BQ76940_BAL_ACTION_NONE)
    {
        return 0U;
    }

    bal = BqHalBalance_Get();
    if (bal == 0)
    {
        return 1U;
    }

    /*
     * ApplyHw 阶段只负责硬件动作：
     *   1. 写逻辑均衡 mask
     *   2. 读回逻辑均衡 mask
     *   3. 校验（写 == 读）
     *
     * 注意：
     *   该函数会访问 BQ76940 I2C。
     *   在 FreeRTOS 任务中调用前，外层必须持有 i2c mutex。
     */
    ret = BqHalBalance_Write(bal, &req->wr);
    if (ret != BQHAL_BALANCE_OK)
    {
        return 2U;
    }

    ret = BqHalBalance_Read(bal, &req->rd);
    if (ret != BQHAL_BALANCE_OK)
    {
        return 3U;
    }

    if (BqHalBalanceMask_Equal(&req->wr, &req->rd) == 0U)
    {
        return 4U;
    }

    /*
     * Write + Read + Verify 全部成功：
     * 更新上一次已确认 mask，供"mask 不一致才写"语义使用。
     */
    s_last_confirmed_mask = req->rd;
    s_last_confirmed_valid = 1U;

    return 0U;
}

uint8_t BQ76940_AppBalanceCommit(BQ76940_AppCtx_t *ctx,
                                 const BQ76940_BalanceRequest_t *req)
{
    BqHalBalance_t *bal;

    if ((ctx == 0) || (req == 0))
    {
        return 1U;
    }

    /*
     * 没有动作时，不修改 app。
     */
    if (req->action == BQ76940_BAL_ACTION_NONE)
    {
        return 0U;
    }

    bal = BqHalBalance_Get();

    if (req->action == BQ76940_BAL_ACTION_START)
    {
        ctx->bal_active = 1U;
        ctx->bal_target_label = (bal != 0)
            ? BqHalBalance_LogicalToLabel(bal, req->target_logical)
            : 0U;
        ctx->bal_target_count = req->target_count;

        BMS_LOG_BALANCE("[BAL] Set Target:VC%d Count:%d WrMask:%08X RdMask:%08X\r\n",
                        ctx->bal_target_label,
                        req->target_count,
                        req->wr.logical_mask,
                        req->rd.logical_mask);
    }
    else if (req->action == BQ76940_BAL_ACTION_STOP)
    {
        ctx->bal_active = 0U;
        ctx->bal_target_label = 0U;
        ctx->bal_target_count = 0U;
        ctx->bal_last_refresh_ms = 0U;
        ctx->bal_parity_phase = 0U;

        /*
         * STOP 后清除已确认 mask，防止与下一次 START 混淆。
         */
        s_last_confirmed_valid = 0U;

        BMS_LOG_BALANCE("[BAL] off:%d\r\n", req->reason);
    }

    return 0U;
}
