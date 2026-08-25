#ifndef __BMS_UART_HOST_H
#define __BMS_UART_HOST_H

#include <stdint.h>

/* BQ76940_AppCtx 前置声明；本 Adapter 头文件不依赖 bq76940_app.h */
struct BQ76940_AppCtx;

/* Adapter 内部分帧状态不透明，仅暴露三个接口 */
void BMS_UartHost_Init(void);
uint8_t BMS_UartHost_RxProcess(struct BQ76940_AppCtx *ctx);
uint8_t BMS_UartHost_TxPeriodic(struct BQ76940_AppCtx *ctx);

#endif /* __BMS_UART_HOST_H */
