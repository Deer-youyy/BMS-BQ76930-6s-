#ifndef __USART_H__
#define __USART_H__

#include "sys.h"

#define UART_EOK       0
#define UART_ERROR     1
#define UART_ETIMEOUT  2
#define UART_ENAL      3

/*
 * UART1 Driver (Migration-005).
 * Provides only: init, byte-level TX, and byte-level RX via a ring buffer.
 * Frame parse / protocol / business all live in the UART Service layer.
 */
void uart1_init(uint32_t baudrate);

/* Blocking polled transmit of a byte array. */
void uart1_send_bytes(const uint8_t *data, uint16_t len);

/* RX ring: number of bytes currently available. */
uint16_t uart1_available(void);

/* RX ring: read one byte (returns 1 on success, 0 if empty). */
uint8_t uart1_read_byte(uint8_t *byte);

#endif
