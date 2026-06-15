#ifndef __KEY_TASK_H_
#define __KEY_TASK_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void key_module_init(void);
bool key_module_send(uint8_t key_val);
bool key_module_receive(uint8_t *key_val, TickType_t timeout);

QueueHandle_t key_module_get_queue(void);

void key_task(void *pvParameters);

#endif