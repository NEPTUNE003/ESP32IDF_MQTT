#ifndef __LED_TASK_H_
#define __LED_TASK_H_

#include <stdbool.h>
#include "led.h" 

void led_task(void *pvParameters);
void led_set_active(bool active);
void led_next_mode(void);
void led_prev_mode(void);
void led_mode_set(led_mode_t mode);
led_mode_t led_mode_get(void);

#endif