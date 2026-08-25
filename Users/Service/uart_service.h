#ifndef __UART_SERVICE_H__
#define __UART_SERVICE_H__

#include <stdint.h>

/*
 * UART Service (Migration-005).
 * Consumes bytes from the UART1 driver RX ring buffer, assembles frames,
 * checks length / timeout, and posts complete frames to a FreeRTOS queue
 * consumed by the BMS UART Host.
 * This module does NOT operate BQ76930, FET, or any app context.
 */

#define UART_FRAME_MAX_LEN    16U

typedef struct
{
    uint16_t len;
    uint8_t  data[UART_FRAME_MAX_LEN];
} UART_Frame_t;

/* Bind RX queue (queue of UART_Frame_t) and reset state. */
uint8_t UART_Service_Init(void *queue_handle);

/* Service task entry. */
void UART_Service_Task(void *argument);

#endif /* __UART_SERVICE_H__ */
