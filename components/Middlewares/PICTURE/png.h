#ifndef __PNG_H
#define __PNG_H

#include <stdint.h>
#include <stdbool.h>
#include "pngle.h"
#include "piclib.h"
#include "ff.h"
#include "esp_log.h"


/* 函数声明 */
void png_init(pngle_t *pngle, uint32_t w, uint32_t h);
void png_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]);
void png_finish(pngle_t *pngle);
TickType_t png_decode(const char *filename, int width, int height);

#endif
