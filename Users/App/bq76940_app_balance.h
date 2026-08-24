#ifndef __BQ76940_APP_BALANCE_H
#define __BQ76940_APP_BALANCE_H

#include "bq76940_drv.h"
#include "bq_hal_balance.h"

typedef struct
{
    uint16_t diff_enter_mV;       /* 压差进入阈值 */
    uint16_t diff_exit_mV;        /* 压差退出阈值 */
    uint16_t min_cell_mV;         /* 允许均衡的最低最高单体电压门限 */
    int32_t  max_abs_current_mA;  /* 允许均衡的最大电流绝对值 */

    uint32_t refresh_period_ms;   /* 运行中刷新均衡 mask 的时间间隔 */
    uint8_t parity_enable;        /* 是否启用奇偶窗口分时刷新 */
} BQ76940_BalanceConfig_t;

/*
 * 自动均衡动作类型
 *
 * NONE:
 *   本轮不需要操作 CELLBAL。
 *
 * START:
 *   本轮需要开启某个单体的均衡。
 *
 * STOP:
 *   本轮需要关闭所有自动均衡。
 */
#define BQ76940_BAL_ACTION_NONE      0U
#define BQ76940_BAL_ACTION_START     1U
#define BQ76940_BAL_ACTION_STOP      2U

/*
 * 自动均衡停止原因
 */
#define BQ76940_BAL_REASON_NONE          0U
#define BQ76940_BAL_REASON_NOT_ALLOWED   1U
#define BQ76940_BAL_REASON_DIFF_EXIT     2U

#define BQ76940_BAL_MAX_TARGET_COUNT   3U

/*
 * 自动均衡请求结构体
 *
 * 作用：
 *   把"均衡判断"和"CELLBAL 硬件写入"拆开。
 *
 * 使用流程：
 *   1. Decide  阶段：根据 app 状态生成 req
 *   2. ApplyHw 阶段：根据 req 写 BQ76940 CELLBAL
 *   3. Commit  阶段：将执行结果提交回 app
 *
 * 说明：
 *   wr / rd 已改为中性逻辑均衡 mask（BqHalBalanceMask_t），
 *   不再直接使用 BQ76940_CellBalRegs_t。
 *   target_logical 保存逻辑单体下标（0~8），
 *   最终由 Provider 转换成 VC label 用于 CAN 输出。
 */
typedef struct
{
    uint8_t action;         /* NONE / START / STOP */
    uint8_t target_logical; /* START 时的最高目标逻辑单体下标 */
    uint8_t target_count;   /* START 时本轮均衡目标数量 */
    uint8_t reason;         /* STOP 原因 */

    BqHalBalanceMask_t wr;  /* 准备写入的逻辑均衡 mask */
    BqHalBalanceMask_t rd;  /* 写入后读回的逻辑均衡 mask */
} BQ76940_BalanceRequest_t;

/*
 * BQ76940 兼容性 API（由 BQ76940 Provider 提供）
 *   把中性逻辑 mask 转换回 legacy BQ76940_CellBalRegs_t，
 *   仅用于 AppCtx 兼容字段 / CAN 0x306 快照 / Safe-Off 日志，
 *   不参与 Balance 决策，不暴露给 Neutral Balance API。
 */
void BqHalBalance_GetLegacyCellBalRegs(const BqHalBalanceMask_t *mask,
                                       BQ76940_CellBalRegs_t *regs);

struct BQ76940_AppCtx;

void BQ76940_AppBalanceRequestClear(BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceDecide(struct BQ76940_AppCtx *ctx,
                                 BQ76940_BalanceRequest_t *req, uint32_t now_ms);

uint8_t BQ76940_AppBalanceApplyHw(BQ76940_BalanceRequest_t *req);

uint8_t BQ76940_AppBalanceCommit(struct BQ76940_AppCtx *ctx,
                                 const BQ76940_BalanceRequest_t *req);

#endif
