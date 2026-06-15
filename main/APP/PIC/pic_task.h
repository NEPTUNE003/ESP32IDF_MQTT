#ifndef __PIC_TASK_H
#define __PIC_TASK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void pic_task_init(void);       // 初始化图片浏览（创建gif_task）
void picture_enter(void);       // 进入浏览模式
void picture_next(void);        // 下一张
void picture_prev(void);        // 上一张
void picture_exit(void);        // 退出（释放资源）
void pic_task_start_gif(void);
void pic_task_stop_gif(void);


#ifdef __cplusplus
}
#endif

#endif