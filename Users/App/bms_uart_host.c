#include "bms_uart_host.h"
#include "bms_service.h"
#include "uart_service.h"
#include "uart1.h"
#include "FreeRTOS.h"
#include "queue.h"

#include <stdio.h>
#include <string.h>

#define UART_HOST_CMD_LEN 3U

typedef struct
{
    uint8_t data_on;
    uint8_t init_sent;
    void   *queue;
} BMS_UartHost_State_t;

static BMS_UartHost_State_t s_host;

/* Golden 6S cell labels / positions (VC1/VC2/VC5/VC6/VC7/VC10). */
static const int g_cell_pos[6] = { 0, 20, 40, 60, 80, 100 };

void BMS_UartHost_Init(void)
{
    s_host.data_on = 0U;
    s_host.init_sent = 0U;
    s_host.queue = NULL;
}

void BMS_UartHost_BindQueue(void *queue_handle)
{
    s_host.queue = queue_handle;
}

static void BMS_UartHost_TxLine(const char *s)
{
    uint16_t len = (uint16_t)strlen(s);
    uart1_send_bytes((const uint8_t *)s, len);
}

uint8_t BMS_UartHost_RxProcess(BMS_ServiceContext_t *svc)
{
    UART_Frame_t frame;
    uint8_t cmd;

    if (s_host.queue == NULL)
    {
        return 0U;
    }

    if (xQueueReceive((QueueHandle_t)s_host.queue, &frame, 0U) != pdTRUE)
    {
        return 0U;
    }

    if ((frame.len < UART_HOST_CMD_LEN) ||
        (frame.data[0] != 0x01U) ||
        (frame.data[2] != 0x55U))
    {
        return 0U;
    }

    cmd = frame.data[1];
    switch (cmd)
    {
        case 0x02U: s_host.data_on = 1U; s_host.init_sent = 0U; break;
        case 0x03U: s_host.data_on = 0U; break;
        case 0x04U: (void)BMS_ServiceUartSetDsgEnable(svc, 1U); break;
        case 0x05U: (void)BMS_ServiceUartSetDsgEnable(svc, 0U); break;
        case 0x06U: (void)BMS_ServiceUartSetChgEnable(svc, 1U); break;
        case 0x07U: (void)BMS_ServiceUartSetChgEnable(svc, 0U); break;
        default: break;
    }
    return 1U;
}

static void BMS_UartHost_EmitHeader(void)
{
    BMS_UartHost_TxLine("MODE_CFG(1);DIR(1);FSIMG(2097152,0,0,220,176,0);\r\n");
    BMS_UartHost_TxLine("CLR(61);\r\n");
}

uint8_t BMS_UartHost_TxPeriodic(BMS_ServiceContext_t *svc)
{
    char buf[64];
    BMS_UartTelemetry_t tel;
    uint8_t i;

    if (svc == NULL)
    {
        return 0U;
    }

    if (s_host.data_on == 0U)
    {
        return 0U;
    }

    if (s_host.init_sent == 0U)
    {
        BMS_UartHost_EmitHeader();
        s_host.init_sent = 1U;
    }

    BMS_ServiceGetUartTelemetry(svc, &tel);

    for (i = 0U; i < BMS_CELL_COUNT; i++)
    {
        (void)snprintf(buf, sizeof(buf),
                       "DCV16(0,%d,'CELL%d:%umV',3);\r\n",
                       g_cell_pos[i], i + 1, (unsigned)tel.cell_mv[i]);
        BMS_UartHost_TxLine(buf);
    }

    BMS_UartHost_TxLine("CLR(61);\r\n");

    (void)snprintf(buf, sizeof(buf), "DCV16(0,00,'PACK:%umV',3);\r\n",
                   (unsigned)tel.pack_voltage);
    BMS_UartHost_TxLine(buf);

    (void)snprintf(buf, sizeof(buf), "DCV16(0,20,'SOC:%d%%',3);\r\n", 0);
    BMS_UartHost_TxLine(buf);

    (void)snprintf(buf, sizeof(buf), "DCV16(0,40,'TEMP:%.2fC',3);\r\n",
                   (double)tel.temperature / 100.0);
    BMS_UartHost_TxLine(buf);

    (void)snprintf(buf, sizeof(buf), "DCV16(0,60,'AMP:%dmA',3);\r\n",
                   (int)tel.current);
    BMS_UartHost_TxLine(buf);

    BMS_UartHost_TxLine("DCV24(0,20,'');\r\n");
    BMS_UartHost_TxLine("\r\n");

    return 1U;
}
