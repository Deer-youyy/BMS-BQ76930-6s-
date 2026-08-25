#ifndef __BMS_UART_HOST_H__
#define __BMS_UART_HOST_H__

#include <stdint.h>

/*
 * BMS UART Host (Migration-005).
 * Consumes complete frames from the UART Service queue and executes/updates
 * host-facing logic via the BMS Service neutral interface only.
 * This module does NOT depend on BQ76940_AppCtx_t and performs no register access.
 */

typedef struct BMS_ServiceContext BMS_ServiceContext_t;

void BMS_UartHost_Init(void);
void BMS_UartHost_BindQueue(void *queue_handle);

/* Poll: consume one inbound frame and execute command (call from UART Host task). */
uint8_t BMS_UartHost_RxProcess(BMS_ServiceContext_t *svc);

/* Poll: emit periodic golden telemetry to host (gated by data_on). */
uint8_t BMS_UartHost_TxPeriodic(BMS_ServiceContext_t *svc);

#endif /* __BMS_UART_HOST_H__ */
