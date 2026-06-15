#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "led.h"
#include "als_service.h"

#include "led_task.h"
#include "lcd_task.h"
#include "ap3216c_task.h"

static const char *TAG = "LED_TASK";
static esp_timer_handle_t led_timer = NULL;
static bool led_control_active = false;
static led_mode_t s_led_mode = LED_MODE_OFF;

static void led_blink_callback(void *arg)
{
    LED0_TOGGLE();
}

static const led_mode_t mode_cycle[] = {
    LED_MODE_OFF,
    LED_MODE_ON,
    LED_MODE_BLINK_2S,
    LED_MODE_BLINK_5S,
    LED_MODE_AUTO
};
#define MODE_COUNT (sizeof(mode_cycle)/sizeof(mode_cycle[0]))

static void led_apply_mode(led_mode_t mode)
{
    // 停止当前定时器
    if (led_timer) {
        esp_timer_stop(led_timer);
    }
    // 根据模式控制LED
    switch (mode) {
        case LED_MODE_OFF:
            LED0(1);
            break;
        case LED_MODE_ON:
            LED0(0);
            break;
        case LED_MODE_AUTO:
            LED0(als_get_value() < 200 ? 0 : 1);
            break;
        case LED_MODE_BLINK_2S:
            esp_timer_start_periodic(led_timer, 2000000);
            break;
        case LED_MODE_BLINK_5S:
            esp_timer_start_periodic(led_timer, 5000000);
            break;
        default:
            break;
    }
}

void led_mode_set(led_mode_t mode)
{
    if (mode != s_led_mode) {
        s_led_mode = mode;
        led_apply_mode(s_led_mode);
    }
}

led_mode_t led_mode_get(void)
{
    return s_led_mode;
}

void led_set_active(bool active)
{
    led_control_active = active;
    ESP_LOGI(TAG, "LED control active: %d", active);
}

void led_next_mode(void)
{
    int idx = 0;
    for (int i = 0; i < MODE_COUNT; i++) {
        if (mode_cycle[i] == s_led_mode) {
            idx = i;
            break;
        }
    }

    led_mode_t new_mode = mode_cycle[(idx + 1) % MODE_COUNT];
    led_mode_set(new_mode);   
    lcd_show_led_mode(s_led_mode);
    ESP_LOGI(TAG, "Next mode: %d", new_mode);

}

void led_prev_mode(void)
{
    int idx = 0;
    for (int i = 0; i < MODE_COUNT; i++) {
        if (mode_cycle[i] == s_led_mode) {
            idx = i;
            break;
        }
    }
    led_mode_t new_mode = mode_cycle[(idx - 1 + MODE_COUNT) % MODE_COUNT];
    led_mode_set(new_mode); 
    lcd_show_led_mode(s_led_mode);
    ESP_LOGI(TAG, "Prev mode: %d", new_mode);
}

void led_task(void *pvParameters)
{
    uint32_t last_auto_check = 0;

    led_init();
    
    LED0(1);
    s_led_mode = LED_MODE_OFF;
    led_control_active = false;

    esp_timer_create_args_t timer_args = {
        .callback = &led_blink_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "led_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &led_timer));

    while (1) {
        if (s_led_mode == LED_MODE_AUTO) {
            uint32_t now = esp_timer_get_time() / 1000;
            
            if (now - last_auto_check > 500) {
                uint16_t als_value;

                als_value = als_get_value();

                if (als_value < 200) {
                    LED0(0);
                    ESP_LOGD(TAG, "Auto: Dark (ALS=%d), LED ON", als_value);
                } else {
                    LED0(1);
                    ESP_LOGD(TAG, "Auto: Bright (ALS=%d), LED OFF", als_value);
                }
                
                if (led_control_active) {
                    lcd_update_als_value(als_value);
                }
                
                last_auto_check = now;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}