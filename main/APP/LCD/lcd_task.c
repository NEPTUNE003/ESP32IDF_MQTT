#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "led.h"

#include "als_service.h"

#include "led_task.h"
#include "lcd_task.h"
#include "ap3216c_task.h"


static const char *TAG = "LCD_TASK";
static SemaphoreHandle_t lcd_ready_semaphore = NULL;

#define ALS_VALUE_X  50
#define ALS_VALUE_Y  170
#define ALS_VALUE_W  100   
#define ALS_VALUE_H  20    

void lcd_update_als_value(uint16_t als)
{
    if (panel_handle == NULL) return;
    
    char buf[32];
    sprintf(buf, "ALS: %d", als);
    
    // 先用背景色（白色）清除旧数值区域
    spilcd_fill(ALS_VALUE_X, ALS_VALUE_Y, 
                ALS_VALUE_X + ALS_VALUE_W, ALS_VALUE_Y + ALS_VALUE_H, WHITE);
    // 再绘制新数值
    spilcd_show_string(ALS_VALUE_X, ALS_VALUE_Y, 240, 240, 16, buf, BLACK);
}

void lcd_set_led_mode(led_mode_t mode)
{
    led_mode_set(mode);                // 设置 LED 状态（应用硬件）
    lcd_show_led_mode(mode);           // 刷新显示
}

void lcd_show_led_mode(led_mode_t mode)
{
    if (panel_handle == NULL) {
        ESP_LOGW(TAG, "Panel not ready yet");
        return;
    }
    
    spilcd_clear(WHITE); 
    char buf[32];
    
    // 标题
    spilcd_show_string(80, 10, 240, 240, 24, "LED Control", BLUE);
    
    // 显示当前模式
    switch (mode)
    {
        case LED_MODE_OFF:
            spilcd_show_string(50, 130, 240, 240, 24, "Mode: OFF", BLACK);
            break;
        case LED_MODE_ON:
            spilcd_show_string(50, 130, 240, 240, 24, "Mode: ON", RED);
            break;
        case LED_MODE_BLINK_2S:
            spilcd_show_string(50, 130, 240, 240, 24, "Mode: BLINK 2S", BLUE);
            break;
        case LED_MODE_BLINK_5S:
            spilcd_show_string(50, 130, 240, 240, 24, "Mode: BLINK 5S", GREEN);
            break;
        case LED_MODE_AUTO:
            spilcd_show_string(50, 130, 240, 240, 24, "Mode: AUTO", MAGENTA);
            sprintf(buf, "ALS: %d", als_get_value());
            spilcd_show_string(50, 170, 240, 240, 16, buf, BLACK);
            break;
        default:
            break;
    }
}

void lcd_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LCD task start");

    lcd_ready_semaphore = xSemaphoreCreateBinary();
    if (lcd_ready_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create LCD ready semaphore");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    esp_err_t ret = spilcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %d", ret);
        // 即使失败也要释放信号量，避免死锁
        if (lcd_ready_semaphore) {
            xSemaphoreGive(lcd_ready_semaphore);
        }
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // LCD 初始化完成，释放信号量通知 main 任务
    if (lcd_ready_semaphore) {
        xSemaphoreGive(lcd_ready_semaphore);
        ESP_LOGI(TAG, "LCD ready semaphore given");
    }
    ESP_LOGI(TAG, "LCD ready");
    
    // LCD 初始化后，让菜单任务来控制显示内容
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void lcd_wait_ready(void)
{
    if (lcd_ready_semaphore != NULL) {
        xSemaphoreTake(lcd_ready_semaphore, portMAX_DELAY);
    } else {
        ESP_LOGE(TAG, "lcd_wait_ready called but semaphore not created");
    }
}