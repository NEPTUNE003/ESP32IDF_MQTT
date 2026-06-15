#ifndef __AP3216C_TASK_H
#define __AP3216C_TASK_H

#include "freertos/FreeRTOS.h"
#include "ap3216c.h"

extern uint16_t g_ps;                // 接近距离
extern uint16_t g_ir;                // 红外值

void ap3216c_task(void *pvParameters);

#endif