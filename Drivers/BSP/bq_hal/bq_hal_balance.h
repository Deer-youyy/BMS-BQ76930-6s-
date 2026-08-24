#ifndef __BQ_HAL_BALANCE_H
#define __BQ_HAL_BALANCE_H

/* ****************************************************
 * BQ HAL 中性 Balance 访问接口
 *
 * 用途：
 *   本头文件只定义"硬件无关"的逻辑均衡 mask 及访问接口，
 *   不出现任何 BQ76940 / CELLBAL / VC label / 寄存器地址 / 位号。
 *
 * 设计原则：
 *   1. Balance App 只通过本接口操作逻辑均衡 mask，
 *      不直接接触任何 BQ 硬件细节。
 *   2. 逻辑 mask：bit i = 逻辑单体 i，当前 9 个逻辑单体使用 bit 0~8。
 *   3. CELLBAL 位映射与 VC label 映射全部封装在 Provider。
 * **************************************************** */

#include <stdint.h>
#include "../../../Core/bms_types.h"   /* BMS_CELL_COUNT */

/* 不透明 Provider 句柄（由 Provider 定义，通过 Get 获得）*/
typedef struct BqHalBalance BqHalBalance_t;

/* 逻辑均衡 mask：bit i = 逻辑单体 i，当前 9 个逻辑单体使用 bit 0~8。*/
typedef struct
{
    uint32_t logical_mask;
} BqHalBalanceMask_t;

/* 中性错误码 */
typedef enum
{
    BQHAL_BALANCE_OK = 0,          /* 成功 */
    BQHAL_BALANCE_ERR_INVALID,     /* 无效参数：mask 为空 / logical index 越界 */
    BQHAL_BALANCE_ERR_WRITE,       /* 硬件写失败 */
    BQHAL_BALANCE_ERR_READ,        /* 硬件读失败 */
    BQHAL_BALANCE_ERR_VERIFY       /* 回读校验不一致 */
} BqHalBalanceErr_t;

/* Provider 生命周期：加载默认 Provider，获取 Provider 句柄 */
void BqHalBalance_LoadDefault(void);
BqHalBalance_t *BqHalBalance_Get(void);

/* Mask 操作 */
void BqHalBalanceMask_Clear(BqHalBalanceMask_t *m);
uint8_t BqHalBalanceMask_SetLogical(BqHalBalanceMask_t *m, uint8_t logical_idx);
uint8_t BqHalBalanceMask_IsEmpty(const BqHalBalanceMask_t *m);
void BqHalBalanceMask_Or(BqHalBalanceMask_t *dst, const BqHalBalanceMask_t *src);
uint8_t BqHalBalanceMask_Equal(const BqHalBalanceMask_t *a, const BqHalBalanceMask_t *b);

/* 硬件能力：写 / 读（内部串行访问 BQ76940 I2C，调用方须持有 I2C 总线互斥）*/
uint8_t BqHalBalance_Write(BqHalBalance_t *bal, const BqHalBalanceMask_t *m);
uint8_t BqHalBalance_Read(BqHalBalance_t *bal, BqHalBalanceMask_t *m);

/* 逻辑单体 → 标签：返回 VC label（由 Provider 持有 VC label 映射表）*/
uint8_t BqHalBalance_LogicalToLabel(BqHalBalance_t *bal, uint8_t logical_idx);

#endif /* __BQ_HAL_BALANCE_H */
