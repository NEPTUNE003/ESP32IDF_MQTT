#include "qma6100p_task.h"
#include "qma6100p.h"

volatile float g_pitch = 0;
volatile float g_roll = 0;

void accel_task(void *pvParameters)
{
    qma6100p_rawdata_t rawdata;
    qma6100p_init();   // 初始化传感器

    while (1) {
        qma6100p_read_rawdata(&rawdata);
        g_pitch = rawdata.pitch;
        g_roll  = rawdata.roll;
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms 采样一次
    }
}