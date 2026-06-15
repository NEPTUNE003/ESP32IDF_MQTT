#ifndef __LCD_TASK_H
#define __LCD_TASK_H

#include "lcd.h"
#include "led.h"

void lcd_task(void *pvParameters);
void lcd_set_led_mode(led_mode_t mode);
void lcd_show_led_mode(led_mode_t mode);   
void lcd_update_als_value(uint16_t als);   // 仅更新 ALS 数值区域
void lcd_wait_ready(void);

#endif