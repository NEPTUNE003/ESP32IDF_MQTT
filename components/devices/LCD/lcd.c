#include "lcd.h"
#include "lcdfont.h"
#include "spi.h"

esp_lcd_panel_handle_t panel_handle = NULL;
_spilcd_dev spilcddev;

static SemaphoreHandle_t lcd_mutex = NULL;   // 添加互斥锁

#define SPI_LCD_TYPE    1           /* SPI接口屏幕类型（1：2.4寸SPILCD  0：1.3寸SPILCD） */ 

/* LCD的宽和高定义 */
#if SPI_LCD_TYPE                    /* 2.4寸SPI_LCD屏幕 */
uint16_t spilcd_width  = 320;       /* 屏幕的宽度 320(横屏) */
uint16_t spilcd_height = 240;       /* 屏幕的宽度 240(横屏) */
#else
uint16_t spilcd_width  = 240;       /* 屏幕的宽度 240(横屏) */
uint16_t spilcd_height = 240;       /* 屏幕的宽度 240(横屏) */
#endif

/* ===== 静态缓冲区，避免频繁 malloc/free ===== */
#define CHAR_BUF_MAX_W  16  /* 最大字体 32px 时字符宽度为 16 */
static uint16_t char_line_buf[CHAR_BUF_MAX_W];

#define CIRCLE_PTS_MAX  1600  /* 最大半径 160 时 8*160=1280点，每点 x,y = 3200 个 uint16_t */
static uint16_t circle_pts[CIRCLE_PTS_MAX * 2];

static bool notify_lcd_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    return false;
}

esp_err_t spilcd_init(void)
{
    lcd_mutex = xSemaphoreCreateRecursiveMutex();

    if (lcd_mutex == NULL) {
        ESP_LOGE("LCD", "Failed to create LCD mutex");
        return ESP_ERR_NO_MEM;
    }
    board_lcd_reset(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    board_lcd_reset(false);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num         = BOARD_LCD_DC_PIN,
        .cs_gpio_num         = BOARD_LCD_CS_PIN,
        .pclk_hz             = 20 * 1000 * 1000,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
    };
    
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)MY_SPI_HOST, &io_config, &panel_io);
    if (ret != ESP_OK) {
        ESP_LOGE("LCD", "Failed to create panel IO: %d", ret);
        return ret;
    }

    spilcddev.pheight = spilcd_height;
    spilcddev.pwidth  = spilcd_width;

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order  = COLOR_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian    = LCD_RGB_DATA_ENDIAN_BIG,
    };
    
    ret = esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("LCD", "Failed to create ST7789 panel: %d", ret);
        return ret;
    }
    
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lcd_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, NULL);

    spilcd_display_dir(1);  // 确保这个函数存在
    
    spilcd_clear(WHITE);
    board_lcd_power(true); 
    return ESP_OK;
}

void spilcd_display_dir(uint8_t dir)
{
    spilcddev.dir = dir;

    if (spilcddev.dir == 0)         /* 竖屏 */
    {
        spilcddev.width = spilcddev.pheight;
        spilcddev.height = spilcddev.pwidth;
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);

        if (SPI_LCD_TYPE == 0)
        {
            esp_lcd_panel_set_gap(panel_handle, 0, 80);
        }
    }
    else if (spilcddev.dir == 1)    /* 横屏 */
    {
        spilcddev.width = spilcddev.pwidth;
        spilcddev.height = spilcddev.pheight;
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);

        if (SPI_LCD_TYPE == 0)
        {
            esp_lcd_panel_set_gap(panel_handle, 80, 0);
        }
    }
}

void spilcd_clear(uint16_t color)
{
    lcd_lock();

    if (panel_handle == NULL) {
        ESP_LOGE("TAG", "Panel handle is NULL!");
        lcd_unlock();
        return;
    }
    
    uint16_t color_tmp = ((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8);
    
    // 使用合理的缓冲区大小，从 PSRAM 分配
    uint16_t buffer_rows = 40;  // 40 行一次，320*40*2 = 25600 字节
    uint16_t *buffer = heap_caps_malloc(spilcddev.width * sizeof(uint16_t) * buffer_rows, MALLOC_CAP_SPIRAM);
    
    if (NULL == buffer) {
        ESP_LOGW("TAG", "PSRAM allocation failed, trying internal RAM");
        buffer = heap_caps_malloc(spilcddev.width * sizeof(uint16_t) * buffer_rows, MALLOC_CAP_INTERNAL);
    }
    
    if (NULL == buffer) {
        ESP_LOGE("TAG", "Cannot allocate memory for clear");
        lcd_unlock();
        return;
    }
    
    // 填充缓冲区
    for (uint32_t i = 0; i < spilcddev.width * buffer_rows; i++) {
        buffer[i] = color_tmp;
    }
    
    // 逐块绘制（无延时：esp_lcd_panel_draw_bitmap 本身已排队到 DMA，不需要阻塞等待）
    for (uint16_t y = 0; y < spilcddev.height; y += buffer_rows) {
        uint16_t end_y = y + buffer_rows;
        if (end_y > spilcddev.height) end_y = spilcddev.height;
        
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, 0, y, spilcddev.width, end_y, buffer);
        if (ret != ESP_OK) {
            ESP_LOGE("TAG", "Draw bitmap failed: %d", ret);
        }
    }
    
    heap_caps_free(buffer);

    lcd_unlock();
}


/**
 * @brief       在指定区域内填充单个颜色（无锁版本，调用者需持有锁）
 * @param       (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex - sx + 1) * (ey - sy + 1)
 * @param       color:要填充的颜色
 * @retval      无
 */
void spilcd_fill_nolock(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    if (panel_handle == NULL) return;

    /* 计算填充区域宽度 */
    uint16_t width = ex - sx;
    uint16_t height = ey - sy;

    /* 分配内存 */
    uint16_t *buffer = heap_caps_malloc(width * sizeof(uint16_t), MALLOC_CAP_INTERNAL);
    uint16_t color_tmp = ((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8);   /* 需要转换一下颜色值 */

    if (NULL == buffer)
    {
        ESP_LOGE("TAG", "Memory for bitmap is not enough");
        return;  /* 已持有锁，不能在此 unlock */
    }

    /* 填充颜色 */
    for (uint16_t i = 0; i < width; i++)
    {
        buffer[i] = color_tmp;
    }

    /* 绘制填充区域 */
    for (uint16_t y = 0; y < height; y++)
    {
        esp_lcd_panel_draw_bitmap(panel_handle, sx, sy + y, ex, sy + y + 1, buffer);
    }

    /* 释放内存 */
    heap_caps_free(buffer);
}

/**
 * @brief       在指定区域内填充单个颜色（公开版本，自动加锁）
 */
void spilcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    lcd_lock();
    spilcd_fill_nolock(sx, sy, ex, ey, color);
    lcd_unlock();
}

/**
 * @brief       绘画一个像素点
 * @param       x    : x轴坐标
 * @param       y    : y轴坐标
 * @param       color: 颜色值
 * @retval      无
 */
void spilcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_lock();

    uint16_t color_tmp = ((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8);   /* 需要转换一下颜色值 */
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &color_tmp);

    lcd_unlock();
}

/**
 * @brief       画线函数(直线、斜线)
 * @param       x1,y1   起点坐标
 * @param       x2,y2   终点坐标
 * @param       color 填充颜色
 * @retval      无
 */
void spilcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_lock();
    
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0)
    {
        incx = 1;
    }
    else if (delta_x == 0)
    {
        incx = 0;
    }
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0)
    {
        incy = 1;
    }
    else if (delta_y == 0)
    {
        incy = 0;
    }
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if ( delta_x > delta_y)
    {
        distance = delta_x;
    }
    else
    {
        distance = delta_y;
    }

    for (t = 0; t <= distance + 1; t++)
    {
        spilcd_draw_point(row, col, color);
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if (yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }

    lcd_unlock();
}

/**
 * @brief       画水平线
 * @param       x,y: 起点坐标
 * @param       len : 线长度
 * @param       color: 线的颜色
 * @retval      无
 */
void spilcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    lcd_lock();

    /* 参数合法性检查 */
    if (len == 0 || x >= spilcddev.width || y >= spilcddev.height) {
        lcd_unlock();
        return;
    }

    /* 计算终点坐标（不超出屏幕边界） */
    uint16_t ex = fmin(spilcddev.width - 1, x + len - 1);
    uint16_t ey = y;

    uint32_t width = ex - x + 1;
    uint32_t h = 1;  // 水平线只有一行

    /* 分配内存（必须使用内部RAM，因为LCD DMA要求） */
    uint16_t *color_buffer = heap_caps_malloc(width * h * sizeof(uint16_t), MALLOC_CAP_INTERNAL);
    if (color_buffer == NULL) {
        ESP_LOGE("LCD", "Failed to allocate memory for horizontal line");
        lcd_unlock();
        return;
    }

    /* 颜色字节序转换（LCD配置为RGB_BIG_ENDIAN） */
    uint16_t color_tmp = ((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8);

    /* 填充缓冲区 */
    for (uint32_t i = 0; i < width * h; i++) {
        color_buffer[i] = color_tmp;
    }

    /* 绘制水平线 */
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, ex + 1, ey + 1, color_buffer);

    /* 释放内存和互斥锁 */
    heap_caps_free(color_buffer);
    lcd_unlock();
}

/**
 * @brief       画一个矩形
 * @param       x1,y1   起点坐标
 * @param       x2,y2   终点坐标
 * @param       color 填充颜色
 * @retval      无
 */
void spilcd_draw_rectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,uint16_t color)
{
    spilcd_draw_line(x0, y0, x1, y0,color);
    spilcd_draw_line(x0, y0, x0, y1,color);
    spilcd_draw_line(x0, y1, x1, y1,color);
    spilcd_draw_line(x1, y0, x1, y1,color);
}


/**
 * @brief       画一个空心圆（无锁版本，调用者需持有锁）
 * @param       x0,y0 : 圆心坐标
 * @param       r     : 圆半径
 * @param       color : 线条颜色
 * @retval      无
 */
void spilcd_draw_circle_nolock(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    if (panel_handle == NULL) return;

    // 计算圆周上点的最大数量（8 * r 足够）
    uint16_t max_points = 8 * r;
    if (max_points > CIRCLE_PTS_MAX) {
        ESP_LOGE("LCD", "Circle radius too large: %d (max %d)", r, CIRCLE_PTS_MAX / 8);
        return;
    }

    uint16_t *points = circle_pts;  /* 使用静态缓冲区，避免每次 malloc/free */
    int x = r, y = 0;
    int err = 0;
    uint16_t idx = 0;

    // Bresenham 圆弧算法，收集八个对称点
    while (x >= y) {
        points[idx * 2]     = x0 + x; points[idx * 2 + 1] = y0 + y; idx++;
        points[idx * 2]     = x0 + y; points[idx * 2 + 1] = y0 + x; idx++;
        points[idx * 2]     = x0 - y; points[idx * 2 + 1] = y0 + x; idx++;
        points[idx * 2]     = x0 - x; points[idx * 2 + 1] = y0 + y; idx++;
        points[idx * 2]     = x0 - x; points[idx * 2 + 1] = y0 - y; idx++;
        points[idx * 2]     = x0 - y; points[idx * 2 + 1] = y0 - x; idx++;
        points[idx * 2]     = x0 + y; points[idx * 2 + 1] = y0 - x; idx++;
        points[idx * 2]     = x0 + x; points[idx * 2 + 1] = y0 - y; idx++;

        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }

    uint16_t color_swapped = ((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8);
    for (int i = 0; i < idx; i++) {
        uint16_t px = points[i * 2];
        uint16_t py = points[i * 2 + 1];
        if (px < spilcddev.width && py < spilcddev.height) {
            esp_lcd_panel_draw_bitmap(panel_handle, px, py, px + 1, py + 1, &color_swapped);
        }
    }
}

/**
 * @brief       画一个空心圆（公开版本，自动加锁）
 */
void spilcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
    lcd_lock();
    spilcd_draw_circle_nolock(x0, y0, r, color);
    lcd_unlock();
}

/**
 * @brief       显示单个字符（无锁版本，调用者需持有锁）
 * @param       x,y  : 起始坐标
 * @param       chr  : 字符
 * @param       size : 字体大小 12/16/24/32
 * @param       mode : 叠加模式
 * @param       color: 颜色值
 * @retval      无
 */
void spilcd_show_char_nolock(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint8_t mode, uint16_t color)
{
    if (panel_handle == NULL) return;

    const uint8_t *ch_code;
    uint8_t ch_width, ch_height;
    uint8_t ch_offset = chr - ' ';
    
    switch (size) {
        case 12: ch_code = asc2_1206[ch_offset]; ch_width = 6; ch_height = 12; break;
        case 16: ch_code = asc2_1608[ch_offset]; ch_width = 8; ch_height = 16; break;
        case 24: ch_code = asc2_2412[ch_offset]; ch_width = 12; ch_height = 24; break;
        case 32: ch_code = asc2_3216[ch_offset]; ch_width = 16; ch_height = 32; break;
        default: return;
    }

    /* 使用静态栈缓冲区，避免每次 malloc/free */
    uint16_t *line_buf = char_line_buf;  /* 最大宽度 16，足够容纳所有字体 */

    uint16_t color_fg = ((color & 0x00FF) << 8) | ((color & 0xFF00) >> 8);
    uint16_t color_bg = 0xFFFF; // 白色背景

    for (uint8_t row = 0; row < ch_height; row++) {
        // 填充整行背景色
        for (uint8_t col = 0; col < ch_width; col++) {
            line_buf[col] = color_bg;
        }
        // 根据点阵数据覆盖前景色
        uint8_t bytes_per_row = (ch_width + 7) / 8;
        for (uint8_t col = 0; col < ch_width; col++) {
            uint8_t byte_idx = row * bytes_per_row + col / 8;
            uint8_t bit_mask = 0x80 >> (col % 8);
            if (ch_code[byte_idx] & bit_mask) {
                line_buf[col] = color_fg;
            }
        }
        // 绘制这一行
        esp_lcd_panel_draw_bitmap(panel_handle, x, y + row, x + ch_width, y + row + 1, line_buf);
    }
}

/**
 * @brief       显示单个字符（公开版本，自动加锁）
 */
void spilcd_show_char(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint8_t mode, uint16_t color)
{
    lcd_lock();
    spilcd_show_char_nolock(x, y, chr, size, mode, color);
    lcd_unlock();
}

/**
 * @brief       m^n函数
 * @param       m,n: 输入参数
 * @retval      m^n次方
 */
uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while(n--) result *= m;

    return result;
}

/**
 * @brief       显示len个数字
 * @param       x,y : 起始坐标
 * @param       num : 数值(0 ~ 2^32)
 * @param       len : 显示数字的位数
 * @param       size: 选择字体 12/16/24/32
 * @retval      无
 */
void spilcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    lcd_lock();

    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)                                               /* 按总显示位数循环 */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;                       /* 获取对应位的数字 */

        if (enshow == 0 && t < (len - 1))                                   /* 没有使能显示,且还有位要显示 */
        {
            if (temp == 0)
            {
                spilcd_show_char_nolock(x + (size / 2)*t, y, ' ', size, 0, color);    /* 显示空格,占位 */
                continue;                                                   /* 继续下个一位 */
            }
            else
            {
                enshow = 1;                                                 /* 使能显示 */
            }

        }

        spilcd_show_char_nolock(x + (size / 2)*t, y, temp + '0', size, 0, color);     /* 显示字符 */
    }
    lcd_unlock();
}

/**
 * @brief       扩展显示len个数字(高位是0也显示)
 * @param       x,y : 起始坐标
 * @param       num : 数值(0 ~ 2^32)
 * @param       len : 显示数字的位数
 * @param       size: 选择字体 12/16/24/32
 * @param       mode: 显示模式
 *              [7]:0,不填充;1,填充0.
 *              [6:1]:保留
 *              [0]:0,非叠加显示;1,叠加显示.
 * @param       color : 数字的颜色;
 * @retval      无
 */
void spilcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    lcd_lock();

    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)                                                           /* 按总显示位数循环 */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;                                   /* 获取对应位的数字 */

        if (enshow == 0 && t < (len - 1))                                               /* 没有使能显示,且还有位要显示 */
        {
            if (temp == 0)
            {
                if (mode & 0X80)                                                        /* 高位需要填充0 */
                {
                    spilcd_show_char_nolock(x + (size / 2)*t, y, '0', size, mode & 0X01, color);  /* 用0占位 */
                }
                else
                {
                    spilcd_show_char_nolock(x + (size / 2)*t, y, ' ', size, mode & 0X01, color);  /* 用空格占位 */
                }
                continue;
            }
            else
            {
                enshow = 1;                                                             /* 使能显示 */
            }
        }
        spilcd_show_char_nolock(x + (size / 2)*t, y, temp + '0', size, mode & 0X01, color);
    }

    lcd_unlock();
}

/**
 * @brief       显示字符串（无锁版本，调用者需持有锁）
 */
void spilcd_show_string_nolock(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    if (panel_handle == NULL || p == NULL) return;

    uint8_t x0 = x;
    uint16_t max_x = width + x;
    uint16_t max_y = height + y;
    
    while (*p != '\0' && *p <= '~' && *p >= ' ')
    {
        if (x >= max_x)
        {
            x = x0;
            y += size;
        }

        if (y >= max_y) break;

        spilcd_show_char_nolock(x, y, *p, size, 0, color);

        x += size / 2;
        p++;
    }
}

/**
 * @brief       显示字符串（公开版本，自动加锁）
 */
void spilcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    lcd_lock();
    spilcd_show_string_nolock(x, y, width, height, size, p, color);
    lcd_unlock();
}

void lcd_lock(void)
{
    if (lcd_mutex != NULL) {
        xSemaphoreTakeRecursive(lcd_mutex, portMAX_DELAY);
    }
}

void lcd_unlock(void)
{
    if (lcd_mutex != NULL) {
        xSemaphoreGiveRecursive(lcd_mutex);
    }
}

bool lcd_trylock(void)
{
    if (lcd_mutex == NULL) return false;
    return xSemaphoreTakeRecursive(lcd_mutex, 0) == pdTRUE;
}

bool lcd_lock_timeout(TickType_t ticks)
{
    if (lcd_mutex == NULL) return false;
    return xSemaphoreTakeRecursive(lcd_mutex, ticks) == pdTRUE;
}