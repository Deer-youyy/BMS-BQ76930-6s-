#ifndef __BQ76930_BALANCE_H
#define __BQ76930_BALANCE_H

/* BQ76930 Balance Provider specific header.
 *
 * Only the BQ76930 Provider and system assembly layer (bms_tasks)
 * include this header. Balance App business code uses the neutral
 * BqHalBalance interface only.
 *
 * The legacy snapshot API converts a neutral logical mask into the
 * BQ76930-specific CELLBAL registers (BQ76930 has only CELLBAL1/2,
 * no CELLBAL3). It feeds CAN 0x306 / Safe-Off / bal_auto_wr|rd.
 */

#include "bq_hal_balance.h"
#include "bq76930_drv.h"

void BqHalBalance_GetLegacyCellBalRegs(const BqHalBalanceMask_t *mask,
                                       BQ76930_CellBalRegs_t *regs);

#endif /* __BQ76930_BALANCE_H */
