#include "board.h"
#include "iic.h"        
#include "spi.h"       
#include "xl9555.h"    
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BOARD";

esp_err_t board_init(void)
{
    ESP_LOGI(TAG, "Board init start");

    // 1. 初始化 I2C 总线（必须最先，因为 XL9555、传感器、音频都依赖）
    ESP_ERROR_CHECK(iic_init());

    // 2. 初始化 SPI 总线（LCD 和 SD 卡需要）
    ESP_ERROR_CHECK(spi_init());

    // 3. 初始化 XL9555（依赖 I2C）
    ESP_ERROR_CHECK(xl9555_init());

    // 4. 初始化板载 LED 引脚
    gpio_config_t led_conf = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led_conf));
    board_led_set(false);  // 默认关闭

    // 5. 其他板级默认状态
    board_lcd_power(true);
    board_lcd_backlight(true);
    board_speaker_enable(false);

    ESP_LOGI(TAG, "Board init done");
    return ESP_OK;
}

/* ----- 电源与外设控制实现 ----- */
static inline void xl9555_set_pin(uint16_t pin, bool on)
{
    xl9555_pin_write(pin, on ? 1 : 0);
}

void board_lcd_power(bool on)
{
    xl9555_set_pin(SLCD_PWR_IO, on);
}

void board_lcd_reset(bool assert)
{
    xl9555_set_pin(SLCD_RST_IO, assert ? 0 : 1);  
}

void board_lcd_backlight(bool on)
{
    xl9555_set_pin(LCD_BL_IO, on);
}

void board_speaker_enable(bool on)
{
    xl9555_set_pin(SPK_EN_IO, on ? 0 : 1);  
    xl9555_set_pin(SPK_EN_IO, on ? 0 : 1);
}

void board_beep_enable(bool on)
{
    xl9555_set_pin(BEEP_IO, on ? 0 : 1);  
}

void board_led_set(bool on)
{
    // 低电平点亮
    gpio_set_level(BOARD_LED_GPIO_PIN, on ? 0 : 1);
}

void *board_get_xl9555_handle(void)
{
    extern i2c_master_dev_handle_t xl9555_get_handle(void);
    return xl9555_get_handle();
}