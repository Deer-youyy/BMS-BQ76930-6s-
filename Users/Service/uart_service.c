#include "uart_service.h"
#include "uart1.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#define UART_FRAME_TIMEOUT_MS 20U

typedef struct
{
    uint8_t  collecting;
    uint8_t  buf[UART_FRAME_MAX_LEN];
    uint16_t idx;
    uint32_t last_byte_tick;
} UART_Service_State_t;

static QueueHandle_t s_rx_queue = NULL;
static UART_Service_State_t s_svc;

static void UART_Service_ResetFrame(UART_Service_State_t *st)
{
    st->collecting = 0U;
    st->idx = 0U;
}

static void UART_Service_FeedByte(UART_Service_State_t *st, uint8_t byte)
{
    /* If not collecting and this byte is not the frame start (0x01), discard. */
    if ((st->collecting == 0U) && (byte != 0x01U))
    {
        return;
    }

    if (st->idx >= UART_FRAME_MAX_LEN)
    {
        UART_Service_ResetFrame(st);
    }

    st->buf[st->idx++] = byte;
    st->collecting = 1U;
    st->last_byte_tick = xTaskGetTickCount();
}

static void UART_Service_TryEmit(UART_Service_State_t *st)
{
    UART_Frame_t frame;
    uint16_t i;

    /* Golden command frame is exactly 3 bytes: 01 cmd 55. */
    if ((st->collecting != 0U) && (st->idx >= 3U))
    {
        frame.len = st->idx;
        for (i = 0U; i < st->idx; i++)
        {
            frame.data[i] = st->buf[i];
        }

        if (s_rx_queue != NULL)
        {
            (void)xQueueSend(s_rx_queue, &frame, (TickType_t)0U);
        }

        UART_Service_ResetFrame(st);
    }
}

uint8_t UART_Service_Init(void *queue_handle)
{
    s_rx_queue = (QueueHandle_t)queue_handle;
    UART_Service_ResetFrame(&s_svc);
    return 0U;
}

void UART_Service_Task(void *argument)
{
    UART_Service_State_t *st = &s_svc;
    uint8_t byte;
    TickType_t now;

    (void)argument;

    for (;;)
    {
        /* Drain the RX ring buffer. */
        while (uart1_available() > 0U)
        {
            if (uart1_read_byte(&byte) != 0U)
            {
                UART_Service_FeedByte(st, byte);
            }
        }

        if (st->collecting != 0U)
        {
            now = xTaskGetTickCount();
            if ((now - st->last_byte_tick) > pdMS_TO_TICKS(UART_FRAME_TIMEOUT_MS))
            {
                /* Timeout: try to emit what we have, else discard partial frame. */
                if (st->idx >= 3U)
                {
                    UART_Service_TryEmit(st);
                }
                else
                {
                    UART_Service_ResetFrame(st);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
