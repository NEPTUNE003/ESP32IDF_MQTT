#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"

#include "board.h"
#include "lcd.h"
#include "ap3216c.h"
#include "menu.h"
#include "qma6100p.h"

#include "qma6100p_task.h"
#include "ap3216c_task.h"
#include "key_task.h"
#include "led_task.h"
#include "lcd_task.h"
#include "onenet_demo.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_task_wdt_deinit();
    
    ESP_LOGI(TAG, "System start...");

    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(board_init());

    key_module_init();   // 内部创建队列
    
    xTaskCreate(lcd_task, "lcd_task", 8192, NULL, 5, NULL);
    xTaskCreate(ap3216c_task, "ap3216c_task", 8192, NULL, 3, NULL);
    xTaskCreate(key_task, "key_task", 8192, NULL, 4, NULL);
    xTaskCreate(led_task, "led_task", 8192, NULL, 2, NULL);
    xTaskCreate(accel_task, "accel_task", 4096, NULL, 3, NULL);

    lcd_wait_ready();   // 阻塞直到 LCD 初始化完成
    ESP_LOGI(TAG, "LCD ready, starting menu task");
    
    xTaskCreate(menu_task, "menu_task", 20480, NULL, 5, NULL);

    ESP_LOGI(TAG, "All tasks created");

    /* 启动 MQTT 连接（含 WiFi 连接） */
    onenet_mqtt_start();
    ESP_LOGI(TAG, "MQTT started");
}