#ifndef __MUSIC_TASK_H
#define __MUSIC_TASK_H

#include <stdbool.h>
#include <stdint.h>

void music_enter(void);                // 进入音乐播放界面（初始化硬件，开始播放）
void music_exit(void);                 // 退出音乐播放界面（停止播放，释放资源）
void music_next(void);                 // 下一首
void music_prev(void);                 // 上一首
void music_playpause(void);            // 暂停/继续播放
bool music_is_playing(void);           // 返回当前是否处于播放状态（用于更新显示）

void music_update(void);               // 刷新显示（由 menu_task 周期性调用）

#endif