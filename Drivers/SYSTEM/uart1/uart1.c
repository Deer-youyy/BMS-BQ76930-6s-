#include "uart1.h"
#include "stdio.h"
#include "string.h"

/*
 * UART1 Driver - SPSC RX Ring Buffer (Migration-005)
 *
 * ISR only writes received bytes into the RX ring buffer.
 * Frame assembly / CRC / timeout / protocol parse all belong to the
 * UART Service layer. This file does NO protocol or business logic.
 */

#define UART1_RING_SIZE 256U

static uint8_t  uart1_ring[UART1_RING_SIZE];
static volatile uint16_t uart1_head = 0U;   /* producer(ISR) write index */
static volatile uint16_t uart1_tail = 0U;   /* consumer(Task) read index */

UART_HandleTypeDef uart1_handle = {0};

static uint16_t UART1_RingCount(void)
{
    return (uint16_t)((uart1_head - uart1_tail + (uint16_t)UART1_RING_SIZE) % (uint16_t)UART1_RING_SIZE);
}

void uart1_init(uint32_t baudrate)
{
    uart1_handle.Instance = USART1;
    uart1_handle.Init.BaudRate = baudrate;
    uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;
    uart1_handle.Init.StopBits = UART_STOPBITS_1;
    uart1_handle.Init.Parity = UART_PARITY_NONE;
    uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart1_handle.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&uart1_handle);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef gpio_initstruct;

        /* TX: PA9, AF push-pull. */
        gpio_initstruct.Pin = GPIO_PIN_9;
        gpio_initstruct.Mode = GPIO_MODE_AF_PP;
        gpio_initstruct.Pull = GPIO_PULLUP;
        gpio_initstruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gpio_initstruct);

        /* RX: PA10, AF input, floating (Golden). */
        gpio_initstruct.Pin = GPIO_PIN_10;
        gpio_initstruct.Mode = GPIO_MODE_AF_INPUT;
        gpio_initstruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_initstruct);

        HAL_NVIC_EnableIRQ(USART1_IRQn);
        HAL_NVIC_SetPriority(USART1_IRQn, 2, 2);

        __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    }
}

void USART1_IRQHandler(void)
{
    uint8_t c;
    uint16_t next;

    if (__HAL_UART_GET_FLAG(&uart1_handle, UART_FLAG_RXNE) != RESET)
    {
        c = (uint8_t)(USART1->DR & 0xFFU);  /* read data, clears RXNE */

        /* producer writes byte into ring (SPSC); only update head */
        next = (uint16_t)((uart1_head + 1U) % (uint16_t)UART1_RING_SIZE);
        if (next != uart1_tail)             /* not full */
        {
            uart1_ring[uart1_head] = c;
            uart1_head = next;
        }
    }
}

int fputc(int ch, FILE *f)
{
    while ((USART1->SR & 0X40U) == 0U) { }
    USART1->DR = (uint8_t)ch;
    return ch;
}

void uart1_send_bytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0U; i < len; i++)
    {
        while ((USART1->SR & USART_SR_TXE) == 0U) { }
        USART1->DR = data[i];
    }
}

uint16_t uart1_available(void)
{
    return UART1_RingCount();
}

uint8_t uart1_read_byte(uint8_t *byte)
{
    if (UART1_RingCount() == 0U)
    {
        return 0U;
    }

    *byte = uart1_ring[uart1_tail];
    uart1_tail = (uint16_t)((uart1_tail + 1U) % (uint16_t)UART1_RING_SIZE);
    return 1U;
}
