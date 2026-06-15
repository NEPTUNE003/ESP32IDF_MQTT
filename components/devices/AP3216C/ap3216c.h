#ifndef __AP3216C_H
#define __AP3216C_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "iic.h"
#include "string.h"

#define AP3216C_INT     xl9555_pin_read(AP_INT_IO)                  /* 读取AP3216C中断状态 */
#define AP3216C_ADDR    0X1E                                        /* AP3216C器件地址 */

esp_err_t ap3216c_init(void);                                       /* 初始化AP3216C */
void ap3216c_read_data(uint16_t *ir, uint16_t *ps, uint16_t *als);  /* 读取AP3216C的数据 */

#endif
