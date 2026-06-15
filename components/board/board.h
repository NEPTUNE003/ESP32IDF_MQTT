#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 引脚映射定义 ========== */

// I2C (与 XL9555、传感器、音频 Codec 共用)
#define BOARD_I2C_NUM        I2C_NUM_0
#define BOARD_I2C_SCL_PIN    GPIO_NUM_42
#define BOARD_I2C_SDA_PIN    GPIO_NUM_41
#define BOARD_I2C_CLK_SPEED  400000

// SPI (LCD 和 SD 卡)
#define BOARD_SPI_HOST       SPI2_HOST
#define BOARD_SPI_SCLK_PIN   GPIO_NUM_12
#define BOARD_SPI_MOSI_PIN   GPIO_NUM_11
#define BOARD_SPI_MISO_PIN   GPIO_NUM_13

// LCD 专用引脚（SPI 接口）
#define BOARD_LCD_DC_PIN     GPIO_NUM_40
#define BOARD_LCD_CS_PIN     GPIO_NUM_21

// SD 卡 CS 引脚
#define BOARD_SD_CS_PIN      GPIO_NUM_2

// LED 引脚
#define BOARD_LED_GPIO_PIN   GPIO_NUM_1

// 板载 BOOT 按键
#define BOARD_BOOT_GPIO_PIN  GPIO_NUM_0

// XL9555 中断引脚
#define BOARD_XL9555_INT_PIN GPIO_NUM_40  
// I2S 音频引脚
#define BOARD_I2S_BCK_IO     GPIO_NUM_46
#define BOARD_I2S_WS_IO      GPIO_NUM_9
#define BOARD_I2S_DO_IO      GPIO_NUM_10
#define BOARD_I2S_DI_IO      GPIO_NUM_14
#define BOARD_I2S_MCK_IO     GPIO_NUM_3

/* ========== 板级初始化 ========== */

/**
 * @brief 板级初始化
 * - 初始化 I2C 总线
 * - 初始化 SPI 总线
 * - 初始化 XL9555（依赖 I2C）
 * - 配置板载 GPIO（如 LED 引脚）
 * - 设置默认电源状态（关闭喇叭、蜂鸣器，LCD 上电等）
 */
esp_err_t board_init(void);

/* ========== 电源与外设控制 ========== */

void board_lcd_power(bool on);      // 控制 LCD 电源（SLCD_PWR_IO）
void board_lcd_reset(bool assert);  // 控制 LCD 硬件复位（SLCD_RST_IO）
void board_lcd_backlight(bool on);  // 控制 LCD 背光（LCD_BL_IO）

void board_speaker_enable(bool on); // 喇叭使能（SPK_EN_IO）
void board_beep_enable(bool on);    // 蜂鸣器控制（BEEP_IO）

void board_led_set(bool on);        // 控制板载 LED

/* ========== 辅助函数 ========== */

/**
 * @brief 获取 XL9555 扩展 GPIO 的句柄（如果外部驱动需要直接操作）
 * 谨慎使用，通常建议通过 board 函数控制。
 */
void *board_get_xl9555_handle(void);  // 返回 i2c_master_dev_handle_t

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */