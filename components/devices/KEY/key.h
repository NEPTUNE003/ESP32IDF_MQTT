#ifndef __KEY_H_
#define __KEY_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdint.h>
#include "board.h"


#define BOOT_GPIO_PIN   BOARD_BOOT_GPIO_PIN
#define BOOT            gpio_get_level(BOOT_GPIO_PIN)       //读取电平
#define BOOT_PRES       5                                 //5代表按下

void key_init(void);
uint8_t key_scan(uint8_t mode);

#endif