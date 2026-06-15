#ifndef ALS_SERVICE_H
#define ALS_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 获取当前 ALS 值
uint16_t als_get_value(void);

// 由 ap3216c_task 调用，更新 ALS 值（内部使用）
void als_update_value(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif