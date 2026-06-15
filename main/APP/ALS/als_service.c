#include "als_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static uint16_t s_als_value = 0;
static portMUX_TYPE s_als_spinlock = portMUX_INITIALIZER_UNLOCKED;

void als_update_value(uint16_t value)
{
    portENTER_CRITICAL(&s_als_spinlock);
    s_als_value = value;
    portEXIT_CRITICAL(&s_als_spinlock);
}

uint16_t als_get_value(void)
{
    uint16_t ret;
    portENTER_CRITICAL(&s_als_spinlock);
    ret = s_als_value;
    portEXIT_CRITICAL(&s_als_spinlock);
    return ret;
}