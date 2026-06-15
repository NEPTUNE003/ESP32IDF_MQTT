#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ap3216c.h"
#include "als_service.h"

#include "ap3216c_task.h"

portMUX_TYPE g_als_spinlock = portMUX_INITIALIZER_UNLOCKED;

uint16_t g_ps = 0;
uint16_t g_ir = 0;

void ap3216c_task(void *pvParameters)
{
    uint16_t ir, ps, als;

    ap3216c_init();

    while (1) {
        ap3216c_read_data(&ir, &ps, &als);
        als_update_value(als);   
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}