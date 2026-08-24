#ifndef __BQ_HAL_H
#define __BQ_HAL_H

/* ****************************************************
 * BQ 硬件抽象层接口（芯片无关）
 *
 * 本层只描述“硬件能力”，不暴露任何具体 BQ 芯片型号、
 * 寄存器地址、RAW 数据或芯片内部结构体。
 * 具体芯片实现由 Hardware Adapter / Concrete Driver 承担。
 * **************************************************** */

#include <stdint.h>

/* 不透明句柄：指向具体硬件适配器实例 */
typedef struct BqHal BqHal_t;

/* 绑定默认硬件适配器（由系统装配层调用） */
void BqHal_Bind(BqHal_t *hal);

/* 获取当前绑定的默认硬件适配器 */
BqHal_t *BqHal_Get(void);

/* FET 能力：表达充电 / 放电 FET 使能意图
 * chg_en：1=充电 FET 导通，0=充电 FET 关断
 * dsg_en：1=放电 FET 导通，0=放电 FET 关断
 * 返回：0=成功，非 0=失败
 */
uint8_t BqHal_ApplyFetEn(BqHal_t *hal, uint8_t chg_en, uint8_t dsg_en);

#endif /* __BQ_HAL_H */