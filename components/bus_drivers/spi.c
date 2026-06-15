#include "spi.h"

/* SD卡设备句柄 */
spi_device_handle_t MY_SD_Handle = NULL;
/* LCD SPI 设备句柄 */
spi_device_handle_t LCD_SPI_Handle = NULL;
// spi.c
spi_device_handle_t sd_spi_device = NULL;

esp_err_t sd_spi_add_device(void)
{
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = BOARD_SD_CS_PIN,
        .queue_size = 7,
    };
    return spi_bus_add_device(MY_SPI_HOST, &devcfg, &sd_spi_device);
}

/**
 * @brief       spi初始化
 * @param       无
 * @retval      esp_err_t
 */
esp_err_t spi_init(void)
{
    spi_bus_config_t buscfg = {
    .sclk_io_num     = BOARD_SPI_SCLK_PIN,
    .mosi_io_num     = BOARD_SPI_MOSI_PIN,
    .miso_io_num     = BOARD_SPI_MISO_PIN,
    .quadwp_io_num   = -1,
    .quadhd_io_num   = -1,
    .max_transfer_sz = 320 * 240 * sizeof(uint16_t),
    };
    
    /* 初始化SPI总线 */
    ESP_ERROR_CHECK(spi_bus_initialize(MY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    return ESP_OK;
}