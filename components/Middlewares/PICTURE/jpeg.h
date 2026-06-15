#ifndef __JPEG_H
#define __JPEG_H

#include <string.h>
#include <unistd.h>
#include "esp_system.h"
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "piclib.h"
#include "pngle.h"
#include "tjpgd.h"


/* rgb565格式 */
typedef uint16_t pixel_jpeg;

/* 函数声明 */
esp_err_t decode_jpeg(pixel_jpeg ***pixels, char * file, int screenWidth, int screenHeight, int * imageWidth, int * imageHeight);
esp_err_t release_image(pixel_jpeg ***pixels, int screenWidth, int screenHeight);
TickType_t jpeg_decode(const char *filename, int width, int height); /* BMP解码 */

#endif
