/**
 * bms_config.h - BMS 6S 通道配置（Migration M01）
 *
 * 6S 物理通道映射（已从实际硬件源码确认，见 源码/BSP/BQ76930.c Get_BatteryX 系列）：
 *
 *   Domain Cell | BQ VC 通道 | Batteryval 索引
 *   ------------|------------|----------------
 *   Cell 0      | VC1        | Batteryval[0]
 *   Cell 1      | VC2        | Batteryval[1]
 *   Cell 2      | VC5        | Batteryval[4]
 *   Cell 3      | VC6        | Batteryval[5]
 *   Cell 4      | VC7        | Batteryval[6]
 *   Cell 5      | VC10       | Batteryval[9]
 *
 * Domain 层只感知 Cell 0..5；BQ VC 通道编号属于硬件实现细节。
 */
#ifndef BMS_CONFIG_H
#define BMS_CONFIG_H

#include "bms_types.h"   /* BMS_CELL_COUNT 等 */

/* Domain Cell N -> Batteryval 数组索引（BQ 稀疏 VC 通道位置） */
static const uint8_t bms_cell_batteryval_index[BMS_CELL_COUNT] =
{
    0,  /* Cell 0 = VC1  */
    1,  /* Cell 1 = VC2  */
    4,  /* Cell 2 = VC5  */
    5,  /* Cell 3 = VC6  */
    6,  /* Cell 4 = VC7  */
    9   /* Cell 5 = VC10 */
};

#endif /* BMS_CONFIG_H */
