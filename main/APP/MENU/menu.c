#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "led.h"
#include "lcd.h"
#include "key.h"
#include "xl9555.h"
#include "menu.h"
#include "circle.h"

#include "lcd_task.h"
#include "key_task.h"
#include "led_task.h"
#include "pic_task.h"
#include "music_task.h"   

static const char *TAG = "MENU";
static screen_type_t current_screen = SCREEN_MAIN_MENU;     // 当前界面
static menu_item_t selected_item = MENU_LED;                // 高亮选项
static QueueHandle_t cloud_cmd_queue = NULL;                // 云端命令队列

static void show_led_screen(void);
static void show_circle_screen(void);

// ===== 全屏显示主菜单 =====
static void show_main_menu(void)
{
    /* 递归锁：整个菜单原子绘制，不被其他 LCD 写入任务打断 */
    lcd_lock();
    spilcd_fill_nolock(0, 0, spilcddev.width, spilcddev.height, WHITE);
    spilcd_show_string_nolock(80, 10, 240, 240, 24, "MAIN MENU", BLUE);

    spilcd_show_string_nolock(30, 60, 240, 240, 16, "1. LED", BLACK);
    spilcd_show_string_nolock(30, 100, 240, 240, 16, "2. CIRCLE", BLACK);
    spilcd_show_string_nolock(30, 140, 240, 240, 16, "3. PICTURE", BLACK);
    spilcd_show_string_nolock(30, 180, 240, 240, 16, "4. MUSIC", BLACK);

    spilcd_show_char_nolock(20, 60 + selected_item * 40, '>', 16, 0, RED);
    lcd_unlock();
}

// ===== 菜单按键处理 =====
static void handle_menu_key(uint8_t key_val)
{
    ESP_LOGI(TAG, "Menu key received: %d", key_val);
    switch (key_val) {
        case BOOT_PRES:  // 5 确认
            if (selected_item == MENU_LED) {
                current_screen = SCREEN_LED_CONTROL;
                show_led_screen();
            } else if (selected_item == MENU_CIRCLE) {
                current_screen = SCREEN_CIRCLE;
                show_circle_screen();
            } else if (selected_item == MENU_PICTURE) {
                current_screen = SCREEN_PICTURE;
                picture_enter();
            } else if (selected_item == MENU_MUSIC) {
                current_screen = SCREEN_MUSIC;
                music_enter();
            }
            break;
        case KEY1_PRES:  // 2 向下移动
        {
            menu_item_t old = selected_item;
            selected_item = (selected_item + 1) % MENU_MAX;
            /* spilcd_show_char 内部已有 lcd_lock，无需再锁 */
            spilcd_show_char(20, 60 + old * 40, ' ', 16, 0, WHITE);
            spilcd_show_char(20, 60 + selected_item * 40, '>', 16, 0, RED);
            break;
        }
        case KEY3_PRES:  // 4 向上移动
        {
            menu_item_t old = selected_item;
            selected_item = (selected_item > 0) ? selected_item - 1 : MENU_MAX - 1;
            /* spilcd_show_char 内部已有 lcd_lock，无需再锁 */
            spilcd_show_char(20, 60 + old * 40, ' ', 16, 0, WHITE);
            spilcd_show_char(20, 60 + selected_item * 40, '>', 16, 0, RED);
            break;
        }
        default:
            break;
    }
}

static void show_led_screen(void)
{
    led_set_active(true);
    lcd_show_led_mode(led_mode_get());
}

static void show_circle_screen(void)
{
    circle_show();
}



static void handle_led_key(uint8_t key_val)
{
    switch (key_val) {
        case KEY1_PRES:  // 2 下一模式
            led_next_mode();
            break;
        case KEY3_PRES:  // 4 上一模式
            led_prev_mode();
            break;
        case KEY2_PRES:  // 3 返回
            led_set_active(false);
            current_screen = SCREEN_MAIN_MENU;
            show_main_menu();
            break;
        default:
            break;
    }
}


static void handle_circle_key(uint8_t key_val)
{
    ESP_LOGI(TAG, "CIRCLE key received: %d", key_val);
    if (key_val == KEY2_PRES) {  // 3 返回
        ESP_LOGI(TAG, "KEY2 pressed, return to main menu");
        current_screen = SCREEN_MAIN_MENU;
        show_main_menu();
    }
}

static void handle_music_key(uint8_t key_val)
{
    switch (key_val) {
        case KEY1_PRES:   // 下一首
            music_next();
            break;
        case KEY3_PRES:   // 上一首
            music_prev();
            break;
        case KEY0_PRES:   // 暂停/播放
            music_playpause();
            break;
        case KEY2_PRES:   // 返回
            music_exit();
            current_screen = SCREEN_MAIN_MENU;
            show_main_menu();
            break;
        default:
            break;
    }
}

// ===== 处理云端命令（在 menu_task 上下文中执行） =====
static void handle_cloud_cmd(cloud_cmd_t *cmd)
{
    if (!cmd) return;

    switch (cmd->type) {
        case CLOUD_CMD_SWITCH_SCREEN:
            ESP_LOGI(TAG, "Cloud cmd: switch screen to %d", cmd->data.screen);

            /* 先退出当前界面 */
            switch (current_screen) {
                case SCREEN_LED_CONTROL:
                    led_set_active(false);
                    break;
                case SCREEN_PICTURE:
                    picture_exit();
                    break;
                case SCREEN_MUSIC:
                    music_exit();
                    break;
                default:
                    break;
            }

            current_screen = cmd->data.screen;

            /* 进入目标界面 */
            switch (cmd->data.screen) {
                case SCREEN_MAIN_MENU:
                    show_main_menu();
                    break;
                case SCREEN_LED_CONTROL:
                    show_led_screen();
                    break;
                case SCREEN_CIRCLE:
                    show_circle_screen();
                    break;
                case SCREEN_PICTURE:
                    picture_enter();
                    break;
                case SCREEN_MUSIC:
                    music_enter();
                    break;
                default:
                    break;
            }
            break;

        case CLOUD_CMD_SET_LED:
            ESP_LOGI(TAG, "Cloud cmd: set LED to %d", cmd->data.led_mode);
            led_mode_set(cmd->data.led_mode);
            lcd_show_led_mode(cmd->data.led_mode);
            led_set_active(true);
            break;

        default:
            break;
    }
}

// ===== 菜单任务 =====

void menu_task(void *pvParameters)
{
    menu_init();
    
    uint8_t key_val;
    cloud_cmd_t cloud_cmd;
    uint32_t last_circle_update = 0;
    const uint32_t CIRCLE_UPDATE_MS = 50;  // 50ms 更新一次圆的位置
    uint32_t last_music_update = 0;
    const uint32_t MUSIC_UPDATE_MS = 200;   // 音乐界面每200ms刷新一次显示

    while (1) {
        /* ★ 优先处理云端命令（非阻塞接收） */
        while (xQueueReceive(cloud_cmd_queue, &cloud_cmd, 0) == pdTRUE) {
            handle_cloud_cmd(&cloud_cmd);
        }

        // 接收按键（超时 2ms）
        if (key_module_receive(&key_val, pdMS_TO_TICKS(2)) == pdTRUE) {
            ESP_LOGI(TAG, "Received key: %d, screen: %d", key_val, current_screen);
            
            switch (current_screen) {
                case SCREEN_MAIN_MENU:
                    handle_menu_key(key_val);
                    break;
                case SCREEN_LED_CONTROL:
                    handle_led_key(key_val);
                    break;
                case SCREEN_CIRCLE:
                    handle_circle_key(key_val);
                    break;
                case SCREEN_PICTURE:
                    if (key_val == KEY1_PRES) {       // 2 下一张
                        picture_next();
                    } else if (key_val == KEY3_PRES) { // 4 上一张
                        picture_prev();
                    } else if (key_val == KEY2_PRES) { // 3 返回
                        picture_exit();
                        current_screen = SCREEN_MAIN_MENU;
                        show_main_menu();
                    }
                    break;
                case SCREEN_MUSIC:
                    handle_music_key(key_val);
                    break;
            }
        }
        
        // 如果当前在圆形界面，周期性更新圆的位置
        if (current_screen == SCREEN_CIRCLE) {
            uint32_t now = esp_timer_get_time() / 1000;
            if ((now - last_circle_update) >= CIRCLE_UPDATE_MS) {
                circle_update();
                last_circle_update = now;
            }
        }

        if (current_screen == SCREEN_MUSIC) {
            uint32_t now = esp_timer_get_time() / 1000;
            if ((now - last_music_update) >= MUSIC_UPDATE_MS) {
                music_update();
                last_music_update = now;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ===== 菜单初始化 =====

void menu_init(void)
{
    /* 创建云端命令队列（深度8，足够用） */
    if (cloud_cmd_queue == NULL) {
        cloud_cmd_queue = xQueueCreate(8, sizeof(cloud_cmd_t));
    }

    /* 延迟一小段时间，确保之前 SPI 传输彻底完成，防止初始菜单绘制残缺 */
    vTaskDelay(pdMS_TO_TICKS(200));

    show_main_menu();
    ESP_LOGI(TAG, "Menu initialized");
}

// ===== 云端远程屏幕切换（非阻塞：通过队列发送给 menu_task） =====
void menu_switch_screen(screen_type_t screen)
{
    if (cloud_cmd_queue == NULL) {
        ESP_LOGW(TAG, "Cloud cmd queue not ready, ignoring screen switch");
        return;
    }
    cloud_cmd_t cmd = {
        .type = CLOUD_CMD_SWITCH_SCREEN,
        .data.screen = screen,
    };
    xQueueSend(cloud_cmd_queue, &cmd, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Queued cloud switch to screen: %d", screen);
}

// ===== 云端LED控制（非阻塞：通过队列发送给 menu_task） =====
void menu_set_led(led_mode_t mode)
{
    if (cloud_cmd_queue == NULL) {
        ESP_LOGW(TAG, "Cloud cmd queue not ready, ignoring LED set");
        return;
    }
    cloud_cmd_t cmd = {
        .type = CLOUD_CMD_SET_LED,
        .data.led_mode = mode,
    };
    xQueueSend(cloud_cmd_queue, &cmd, pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Queued cloud set LED to %d", mode);
}
