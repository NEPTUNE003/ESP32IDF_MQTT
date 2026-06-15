#ifndef __CIRCLE_H
#define __CIRCLE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 显示圆形界面（清屏、绘制黑色圆、提示文字）
 */
void circle_show(void);

/**
 * @brief 圆形界面周期性更新
 */
void circle_update(void);

#endif /* __CIRCLE_H */