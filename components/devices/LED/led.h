#ifndef __LED_H_
#define __LED_H_

#include "driver/gpio.h"
#include "board.h"

#define LED0_GPIO_PIN    BOARD_LED_GPIO_PIN   

//控制LED开关  0亮
#define LED0(x)          do { x ?                                \
                              gpio_set_level(BOARD_LED_GPIO_PIN, 1):  \
                              gpio_set_level(BOARD_LED_GPIO_PIN, 0);  \
                            } while(0)

#define LED0_TOGGLE()    do { gpio_set_level(BOARD_LED_GPIO_PIN, !gpio_get_level(BOARD_LED_GPIO_PIN)); } while(0)

void led_init(void);

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_BLINK_2S,
    LED_MODE_BLINK_5S,
    LED_MODE_AUTO,
    LED_MODE_MAX
} led_mode_t;

#endif