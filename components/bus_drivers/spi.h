#ifndef __SPI_H
#define __SPI_H

#include <unistd.h>
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "board.h"

#define MY_SPI_HOST         BOARD_SPI_HOST

/* 设备句柄 */
extern spi_device_handle_t MY_SD_Handle;
extern spi_device_handle_t sd_spi_device;

/* 函数声明 */
esp_err_t spi_init(void);
esp_err_t sd_spi_add_device(void);

#endif