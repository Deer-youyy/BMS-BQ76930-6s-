#ifndef __IO_CTRL_H
#define __IO_CTRL_H

#include "stm32f10x.h"


#define GPIOA_RCC                 RCC_APB2Periph_GPIOA
#define GPIOA_PORT                GPIOA
#define MCU_WAKE_BQ               GPIO_Pin_8
#define MCU_WAKE_BQ_ONOFF(x)      GPIO_WriteBit(GPIOA_PORT ,MCU_WAKE_BQ,x);

#define GPIOB_RCC                 RCC_APB2Periph_GPIOB
#define GPIOB_PORT                GPIOB
#define PB15                       GPIO_Pin_15
#define PB15_ONOFF(x)              GPIO_WriteBit(GPIOB_PORT ,PB15 ,x);

void IO_CTRL_Config(void);

#endif
