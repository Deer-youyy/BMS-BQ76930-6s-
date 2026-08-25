#include "bms_uart_host.h"
#include "uart1.h"
#include "bq76930_hal.h"
#include "bq76940_app.h"

#include <stdio.h>
#include <string.h>

/*
 * UART1 上位机 Golden 协议 Adapter
 *
 * 下行帧（每周期从 uart1_rx_get_frame 取一帧）：
 *   帧格式 `01 cmd 55`（3 字节）
 *     0x02 开始发送数据（data_on=1）
 *     0x03 停止发送数据（data_on=0）
 *     0x04 打开 DSG
 *     0x05 关闭 DSG
 *     0x06 打开 CHG
 *     0x07 关闭 CHG
 *   CHG/DSG 通过 BQ76930 Provider 层接口执行（BQ76930_HalSetCHG / BQ76930_HalSetDSG）。
 *
 * 上行（按 Golden Update_val() 逐项复现，受 data_on 门控）：
 *   ASCII 文本，每行以 \r\n 结尾，仅使用 uart1_send_bytes。
 */

/* BQ76940_AppCtx_t 前置声明（禁止 include bq76940_app.h） */
struct BQ76940_AppCtx;

/* Adapter 内部状态（不透明） */
typedef struct
{
    uint8_t data_on;   /* 0x02 开始周期发送, 0x03 停止 */
    uint8_t init_sent; /* 每轮开头 MODE_CFG/CLR 是否已发送 */
} BMS_UartHost_State_t;

static BMS_UartHost_State_t s_host;

/* Golden 6S 电压：cell_mV[] -> VC 标签（VC1/VC2/VC5/VC6/VC7/VC10） */
static const char * const g_cell_labels[6] = {
    "第一节", "第二节", "第三节", "第四节", "第五节", "第六节"
};
static const uint8_t g_cell_pos[6] = { 0U, 20U, 40U, 60U, 80U, 100U };

/* 发送一行 ASCII（以 \r\n 结尾） */
static void BMS_UartHost_TxLine(const char *s)
{
    uint16_t len = (uint16_t)strlen(s);
    uart1_send_bytes((const uint8_t *)s, len);
}

void BMS_UartHost_Init(void)
{
    s_host.data_on = 0U;
    s_host.init_sent = 0U;
}

uint8_t BMS_UartHost_RxProcess(struct BQ76940_AppCtx *ctx)
{
    uint8_t frm[8];
    uint16_t n;

    (void)ctx;

    n = uart1_rx_get_frame(frm, sizeof(frm));
    if (n < 3U)
    {
        return 0U;
    }

    if ((frm[0] != 0x01U) || (frm[2] != 0x55U))
    {
        return 0U;
    }

    switch (frm[1])
    {
        case 0x02U: /* 开始发送数据 */
            s_host.data_on = 1U;
            s_host.init_sent = 0U;
            break;
        case 0x03U: /* 停止发送数据 */
            s_host.data_on = 0U;
            break;
        case 0x04U: /* 打开 DSG */
            (void)BQ76930_HalSetDSG(1U);
            break;
        case 0x05U: /* 关闭 DSG */
            (void)BQ76930_HalSetDSG(0U);
            break;
        case 0x06U: /* 打开 CHG */
            (void)BQ76930_HalSetCHG(1U);
            break;
        case 0x07U: /* 关闭 CHG */
            (void)BQ76930_HalSetCHG(0U);
            break;
        default:
            break;
    }

    return 1U;
}

uint8_t BMS_UartHost_TxPeriodic(struct BQ76940_AppCtx *ctx)
{
    char buf[64];
    uint8_t i;

    if (ctx == 0)
    {
        return 0U;
    }

    if (s_host.data_on == 0U)
    {
        return 0U;
    }

    /* 每轮开头仅一次：MODE_CFG + CLR */
    if (s_host.init_sent == 0U)
    {
        BMS_UartHost_TxLine("MODE_CFG(1);DIR(1);FSIMG(2097152,0,0,220,176,0);\r\n");
        BMS_UartHost_TxLine("CLR(61);\r\n");
        s_host.init_sent = 1U;
    }

    /* 6 节电压，偏移 0,20,40,60,80,100 */
    for (i = 0U; i < 6U; i++)
    {
        (void)snprintf(buf, sizeof(buf),
                       "DCV16(0,%u,'%s电压:%dmV',3);\r\n",
                       (unsigned)g_cell_pos[i],
                       g_cell_labels[i],
                       (int)ctx->cell_mV[i]);
        BMS_UartHost_TxLine(buf);
    }

    BMS_UartHost_TxLine("CLR(61);\r\n");

    /* 总电压（mV） */
    (void)snprintf(buf, sizeof(buf), "DCV16(0,00,'总电压:%dmV',3);\r\n",
                   (int)ctx->pack_total_mV);
    BMS_UartHost_TxLine(buf);

    /* SOC：无专用字段，Golden SOC 在 Batteryval[32]，取 0 */
    (void)snprintf(buf, sizeof(buf), "DCV16(0,20,'电池SOC为:%d%',3);\r\n", 0);
    BMS_UartHost_TxLine(buf);

    /* 温度：ts1_temp_dC / 100.0 -> 摄氏度 */
    (void)snprintf(buf, sizeof(buf), "DCV16(0,40,'电池温度为:%.2f℃',3);\r\n",
                   (double)ctx->ts1_temp_dC / 100.0);
    BMS_UartHost_TxLine(buf);

    /* 电流（mA） */
    (void)snprintf(buf, sizeof(buf), "DCV16(0,60,'电流:%dmA',3);\r\n",
                   (int)ctx->pack_current_mA);
    BMS_UartHost_TxLine(buf);

    BMS_UartHost_TxLine("DCV24(0,20,'');\r\n");
    BMS_UartHost_TxLine("\r\n");

    return 1U;
}
