#ifndef __BQ76940_BALANCE_H
#define __BQ76940_BALANCE_H

/* ****************************************************
 * BQ76940 平衡 Provider 专属头
 *
 * 本头只允许被 BQ76940 Provider / 系统装配层（bms_tasks）包含，
 * Balance App 业务代码不得包含本头。
 *
 * 设计原则：
 *   1. BQ76940 专有类型（BQ76940_CellBalRegs_t）只存在于本边界。
 *   2. 中性接口 bq_hal_balance.h 不依赖 BQ76940 任何类型。
 *   3. 本头用于暴露 legacy 快照转换 API，
 *      供 CAN 0x306 / Safe-Off 快照回填 bal_auto_wr / bal_auto_rd 使用。
 * **************************************************** */

#include "bq_hal_balance.h"
#include "bq76940_drv.h"

/* BQ76940 兼容性 API（BQ76940 Provider 实现）
 *   把中性逻辑 mask 转换回 legacy BQ76940_CellBalRegs_t，
 *   仅用于 AppCtx 兼容字段 / CAN 0x306 快照 / Safe-Off 日志，
 *   不参与 Balance 决策，不暴露给 Neutral Balance API。 */
void BqHalBalance_GetLegacyCellBalRegs(const BqHalBalanceMask_t *mask,
                                       BQ76940_CellBalRegs_t *regs);

#endif /* __BQ76940_BALANCE_H */
