#ifndef __PICLIB_H
#define __PICLIB_H

#include "lcd.h"
#include <unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "ff.h"
#include "exfuns.h"
#include "gif.h"
#include "bmp.h"
#include "jpeg.h"
#include "png.h"


#define PIC_FORMAT_ERR      0x27    /* 格式错误 */
#define PIC_SIZE_ERR        0x28    /* 图片尺寸错误 */
#define PIC_WINDOW_ERR      0x29    /* 窗口设定错误 */
#define PIC_MEM_ERR         0x11    /* 内存错误 */

/* 判断 TRUE 和 FALSE 是否已经定义了? 没有则要定义! */
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

#define rgb565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

/* 图片显示物理层接口 */
/* 在移植的时候,必须由用户自己实现这几个函数 */
typedef struct
{
    /* void draw_point(uint16_t x,uint16_t y,uint32_t color) 画点函数 */
    void(*draw_point)(uint16_t, uint16_t, uint16_t);
    
    /* void fill(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint32_t color) 单色填充函数 */
    void(*fill)(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
    
    /* void draw_hline(uint16_t x0,uint16_t y0,uint16_t len,uint16_t color) 画水平线函数 */
    void(*draw_hline)(uint16_t, uint16_t, uint16_t, uint16_t);
    
    /* void piclib_multi_color(uint16_t x, uint16_t y, uint16_t width, uint16_t *color) 多点填充 */
    void(*multicolor)(uint16_t, uint16_t, uint16_t, uint16_t *);
} _pic_phy;

extern _pic_phy pic_phy;


/* 图像信息 */
typedef struct
{
    uint16_t lcdwidth;      /* LCD的宽度 */
    uint16_t lcdheight;     /* LCD的高度 */
} _pic_info;

extern _pic_info picinfo;   /* 图像信息 */

// piclib.h 末尾添加
typedef struct {
    bool is_gif;                // 当前是否为 GIF
    gif89a *gif_info;           // GIF 解码上下文
    FIL *gif_file;              // GIF 文件句柄
    uint16_t gif_frame_delay;   // 帧间延时（单位：10ms）
    uint32_t gif_last_frame_time;
} pic_gif_state_t;

extern pic_gif_state_t g_pic_gif_state;  // 全局 GIF 状态

/* 图片解码库 接口函数 */
void piclib_mem_free (void *paddr);     /* 释放内存 */
void *piclib_mem_malloc (uint32_t size);/* 申请内存 */
void piclib_init(void);                 /* 初始化画图 */
uint8_t piclib_ai_load_picfile(char *filename, uint16_t x, uint16_t y, uint16_t width, uint16_t height); /* 智能画图 */

#endif
