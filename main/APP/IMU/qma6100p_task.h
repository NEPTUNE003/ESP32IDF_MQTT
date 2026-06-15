// qma6100p_task.h
#ifndef __QMA6100P_TASK_H
#define __QMA6100P_TASK_H

#include "qma6100p.h"

extern volatile float g_pitch;   // 俯仰角（度）
extern volatile float g_roll;    // 翻滚角（度）

void accel_task(void *pvParameters);

#endif