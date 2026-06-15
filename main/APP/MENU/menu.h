#ifndef __MENU_H
#define __MENU_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lcd.h"
#include "led_task.h"

/* 菜单项定义 */
typedef enum {
    MENU_LED = 0,
    MENU_CIRCLE,
    MENU_PICTURE,
    MENU_MUSIC,
    MENU_MAX
} menu_item_t;

/* 当前活动界面 */
typedef enum {
    SCREEN_MAIN_MENU,
    SCREEN_LED_CONTROL,
    SCREEN_CIRCLE,
    SCREEN_PICTURE,
    SCREEN_MUSIC
} screen_type_t;

/* 云端下发的命令类型 */
typedef enum {
    CLOUD_CMD_SWITCH_SCREEN,
    CLOUD_CMD_SET_LED,
} cloud_cmd_type_t;

/* 云端命令结构体 */
typedef struct {
    cloud_cmd_type_t type;
    union {
        screen_type_t screen;
        led_mode_t    led_mode;
    } data;
} cloud_cmd_t;

/* 函数声明 */
void menu_init(void);
void menu_task(void *pvParameters);

/* 云端远程屏幕切换（非阻塞，通过队列发送给 menu_task） */
void menu_switch_screen(screen_type_t screen);

/* 云端LED控制（非阻塞，通过队列发送给 menu_task） */
void menu_set_led(led_mode_t mode);

#endif /* __MENU_H */
