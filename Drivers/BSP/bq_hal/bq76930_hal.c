/* ****************************************************
 * BQ76930 硬件适配器（Hardware Adapter）
 *
 * 作用：把芯片无关的 BqHal 能力映射到具体 BQ76930 驱动。
 *      本层允许出现 BQ76930 型号细节，属于 Hardware Adapter 层；
 *      只做能力到驱动 API 的映射，不包含业务策略。
 *
 * 架构：
 *   App / Component
 *        ↓
 *      BqHal
 *        ↓
 *   bq76930_hal.c
 *        ↓
 *   BQ76930 Driver
 *        ↓
 *     Hardware
 * **************************************************** */

#include "bq_hal.h"
#include "bq76930_drv.h"

/* 不透明句柄内部结构：保留给未来扩展，当前无需额外字段 */
struct BqHal
{
    uint32_t model_ver; /* 适配器版本标识，预留给未来区分具体芯片 */
};

static BqHal_t s_bq76930_hal;
static BqHal_t *s_default_hal = 0U;

/* 初始化 BQ76930 适配器并绑定为默认（由系统装配层 main 调用） */
BqHal_t *BQ76930_HalDefault(void)
{
    BqHal_Bind(&s_bq76930_hal);
    return &s_bq76930_hal;
}

void BqHal_Bind(BqHal_t *hal)
{
    s_default_hal = hal;
}

BqHal_t *BqHal_Get(void)
{
    return s_default_hal;
}

uint8_t BqHal_ApplyFetEn(BqHal_t *hal, uint8_t chg_en, uint8_t dsg_en)
{
    if (hal == 0U)
    {
        return 1U; /* 未装配适配器，视为失败 */
    }

    /* 1:1 映射到 BQ76930 驱动 FET 接口，保持原 CHG/DSG 语义 */
    return BQ76930_SetFETState(chg_en, dsg_en);
}
