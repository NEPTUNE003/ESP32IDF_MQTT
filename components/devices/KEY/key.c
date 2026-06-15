#include "key.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "KEY";

void key_init(void)
{
    gpio_config_t gpio_init_struct = {0};

    gpio_init_struct.intr_type      = GPIO_INTR_DISABLE;
    gpio_init_struct.mode           = GPIO_MODE_INPUT;
    gpio_init_struct.pull_up_en     = GPIO_PULLUP_ENABLE;
    gpio_init_struct.pull_down_en   = GPIO_PULLDOWN_DISABLE;
    gpio_init_struct.pin_bit_mask   = 1ull << BOOT_GPIO_PIN;

    ESP_ERROR_CHECK(gpio_config(&gpio_init_struct));
    ESP_LOGI(TAG, "BOOT key initialized on GPIO%d", BOOT_GPIO_PIN);
}

uint8_t key_scan(uint8_t mode)
{
    return (gpio_get_level(BOOT_GPIO_PIN) == 0) ? BOOT_PRES : 0;
}