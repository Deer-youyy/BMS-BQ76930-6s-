#ifndef __BMS_TASKS_H
#define __BMS_TASKS_H

#include "FreeRTOS.h"
#include "bms_service.h"

struct BQ76940_AppCtx;

BaseType_t BMS_TasksCreate(BMS_ServiceContext_t *svc,
                           struct BQ76940_AppCtx *app);

void BMS_HwFaultNotifyFromISR(void);
#endif
