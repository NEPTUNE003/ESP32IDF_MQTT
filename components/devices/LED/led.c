#include "led.h"
#include "esp_log.h"

static const char *TAG = "LED";

void led_init(void)
{
    ESP_LOGI(TAG, "LED software initialized (hardware already configured by board)");
}