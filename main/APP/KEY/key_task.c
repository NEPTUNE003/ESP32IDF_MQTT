#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "key.h"
#include "xl9555.h"

#include "key_task.h"

static const char *TAG = "KEY_TASK";
static QueueHandle_t s_key_queue = NULL;

typedef struct {
    uint8_t last_state;
    uint8_t stable_count;
    uint8_t last_sent;
} debounce_t;

static debounce_t key_deb = {0, 0, 0};
static debounce_t boot_deb = {0, 0, 0};
#define DEBOUNCE_THRESHOLD  3

// 处理单个按键：返回需要发送的按键值（0表示无需发送）
static uint8_t debounce_process(uint8_t current, debounce_t *deb)
{
    if (current == deb->last_state && current != 0) {
        if (++deb->stable_count >= DEBOUNCE_THRESHOLD) {
            // 按键稳定，检查是否已发送过（边沿触发）
            if (current != deb->last_sent) {
                deb->last_sent = current;
                deb->stable_count = 0;
                return current;   // 需要发送
            }
        }
    } else if (current == 0) {
        // 按键释放，重置状态
        deb->stable_count = 0;
        deb->last_sent = 0;
    }
    deb->last_state = current;
    return 0;
}

void key_task(void *pvParameters)
{
    if (s_key_queue == NULL) {
        ESP_LOGE(TAG, "Key queue not initialized, call key_module_init first");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (1)
    {
        // 扫描 XL9555 按键
        uint8_t key_val = xl9555_key_scan(0);
        uint8_t send_key = debounce_process(key_val, &key_deb);
        if (send_key) {
            xQueueSend(s_key_queue, &send_key, 0);   
            ESP_LOGI(TAG, "Send KEY: %d", send_key);
        }

        // 扫描 BOOT 按键
        uint8_t boot_val = key_scan(0);
        uint8_t send_boot = debounce_process(boot_val, &boot_deb);
        if (send_boot) {
            xQueueSend(s_key_queue, &send_boot, 0);
            ESP_LOGI(TAG, "Send BOOT: %d", send_boot);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


// 初始化按键模块：创建队列
void key_module_init(void)
{
    if (s_key_queue == NULL) {
        s_key_queue = xQueueCreate(10, sizeof(uint8_t));
        if (s_key_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create key queue");
        } else {
            ESP_LOGI(TAG, "Key module initialized");
        }
    }
}

// 发送按键值
bool key_module_send(uint8_t key_val)
{
    if (s_key_queue == NULL) return false;
    return xQueueSend(s_key_queue, &key_val, 0) == pdTRUE;
}

// 接收按键值
bool key_module_receive(uint8_t *key_val, TickType_t timeout)
{
    if (s_key_queue == NULL) return false;
    return xQueueReceive(s_key_queue, key_val, timeout) == pdTRUE;
}

// 获取队列句柄（不推荐，但保留兼容）
QueueHandle_t key_module_get_queue(void)
{
    return s_key_queue;
}