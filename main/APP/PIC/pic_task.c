#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "ff.h"
#include "fonts.h"
#include "piclib.h"
#include "exfuns.h"
#include "text.h"
#include "gif.h"
#include "lcd.h"
#include "spi_sd.h"
#include "ff_utils.h"

#include "pic_task.h"

static const char *TAG = "PIC";
static void gif_task(void *pvParameters);
static TaskHandle_t s_gif_task_handle = NULL;

static struct {
    bool active;
    uint16_t total;
    uint16_t index;
    uint32_t *offset_tbl;
    FF_DIR dir;
    FILINFO fileinfo;
    char path[256];
    // GIF 相关状态
    bool is_gif;
    gif89a *gif_info;
    FIL *gif_file;
    uint16_t gif_frame_delay;
    uint32_t gif_last_frame_time;
} pic_state = {0};

// ===== 图片浏览内部函数声明 =====
 void picture_enter(void);
 void picture_show_current(void);
 void picture_next(void);
 void picture_prev(void);
 void picture_exit(void);
 uint16_t pic_get_tnum(char *path);

uint16_t pic_get_tnum(char *path)
{
    return ff_get_file_count(path, 0x50);
}

 void picture_show_current(void)
{
    ff_dir_seek_offset(&pic_state.dir, pic_state.offset_tbl[pic_state.index]);
    f_readdir(&pic_state.dir, &pic_state.fileinfo);
    sprintf(pic_state.path, "0:/PICTURE/%s", pic_state.fileinfo.fname);

    spilcd_clear(BLACK);

    uint8_t file_type = exfuns_file_type(pic_state.fileinfo.fname);
    
    if (file_type == 0x50) {
        // 停止之前的 GIF 动画（如果有）
        if (pic_state.is_gif) {
            gif_decode_stop(pic_state.gif_info, pic_state.gif_file);
            pic_state.is_gif = false;
        }
        // 启动新的 GIF 解码，显示第一帧
        int ret = gif_decode_start(pic_state.path, 0, 0, spilcddev.width, spilcddev.height,
                                   &pic_state.gif_info, &pic_state.gif_file);
        if (ret == 0) {
            pic_state.is_gif = true;
            pic_state.gif_frame_delay = 10;   // 默认延时 100ms
            pic_state.gif_last_frame_time = 0;
        } else {
            text_show_string(2, 2, spilcddev.width, 16, "GIF error", 16, 0, RED);
        }
    } else {
        // 非 GIF 格式：直接调用原图片解码函数（阻塞，但静态图片解码很快）
        piclib_ai_load_picfile(pic_state.path, 0, 0, spilcddev.width, spilcddev.height);
    }

    // 显示文件名
    text_show_string(2, 2, spilcddev.width, 16, pic_state.path, 16, 0, RED);
}

/**
 * @brief       下一张图片
 */
 void picture_next(void)
{
    if (!pic_state.active) return;
    pic_state.index = (pic_state.index + 1) % pic_state.total;
    picture_show_current();
}

/**
 * @brief       上一张图片
 */
 void picture_prev(void)
{
    if (!pic_state.active) return;
    pic_state.index = (pic_state.index == 0) ? pic_state.total - 1 : pic_state.index - 1;
    picture_show_current();
}


 void picture_exit(void)
{
    if (!pic_state.active) return;

    pic_task_stop_gif(); 
    
    // 停止 GIF 并释放资源
    if (pic_state.is_gif) {
        gif_decode_stop(pic_state.gif_info, pic_state.gif_file);
        pic_state.is_gif = false;
    }
    
    f_closedir(&pic_state.dir);
    free(pic_state.offset_tbl);
    memset(&pic_state, 0, sizeof(pic_state));
}

void gif_task(void *pvParameters)
{
    while (1) {
        if (pic_state.active && pic_state.is_gif) {

            uint32_t now = esp_timer_get_time() / 1000;

            if (pic_state.gif_last_frame_time == 0) 
            {
                pic_state.gif_last_frame_time = now;
            }
            if ((now - pic_state.gif_last_frame_time) >= pic_state.gif_frame_delay) 
            {
                uint16_t delay = 10;

                if (!lcd_lock_timeout(pdMS_TO_TICKS(100))) {
                    ESP_LOGW(TAG, "GIF task: LCD mutex timeout, skip frame");
                    vTaskDelay(pdMS_TO_TICKS(5));
                    continue;
                }

                int ret = gif_decode_next_frame(
                    pic_state.gif_info,
                    pic_state.gif_file,
                    0, 0,
                    &delay
                );

                lcd_unlock();

                pic_state.gif_frame_delay = delay;
                pic_state.gif_last_frame_time = now;

                if (ret != 0)
                {
                gif_decode_stop(pic_state.gif_info, pic_state.gif_file);
                pic_state.gif_info = NULL;
                pic_state.gif_file = NULL;
                pic_state.is_gif = false;
                }
             }

        vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}


/**
 * @brief       进入图片浏览模式
 */
void picture_enter(void)
{
    memset(&pic_state, 0, sizeof(pic_state));

    spilcd_clear(BLACK);
    spilcd_show_string(10, 10, 240, 16, 16, "Initializing SD...", WHITE);

    // 挂载 SD 卡（若已挂载则跳过）
    if (sd_spi_init() != ESP_OK) {
        spilcd_show_string(10, 30, 240, 16, 16, "SD Card Error!", RED);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    // 初始化字库
    if (fonts_init() != 0) {
        spilcd_show_string(10, 50, 240, 16, 16, "Font Error!", RED);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    // 初始化图片解码库
    piclib_init();

    // 打开 PICTURE 目录
    if (f_opendir(&pic_state.dir, "0:/PICTURE") != FR_OK) {
        spilcd_show_string(10, 70, 240, 16, 16, "No PICTURE dir!", RED);
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    // 获取图片总数
    pic_state.total = pic_get_tnum("0:/PICTURE");
    if (pic_state.total == 0) {
        spilcd_show_string(10, 90, 240, 16, 16, "No pictures!", RED);
        vTaskDelay(pdMS_TO_TICKS(2000));
        f_closedir(&pic_state.dir);
        return;
    }

    // 建立偏移量表
    pic_state.offset_tbl = malloc(4 * pic_state.total);
    if (!pic_state.offset_tbl) {
        f_closedir(&pic_state.dir);
        return;
    }

    uint16_t cnt = 0;
    f_rewinddir(&pic_state.dir);
    while (cnt < pic_state.total) {
        uint32_t pos = pic_state.dir.dptr;
        f_readdir(&pic_state.dir, &pic_state.fileinfo);
        if (pic_state.fileinfo.fname[0] == 0) break;
        uint8_t type = exfuns_file_type(pic_state.fileinfo.fname);
        if ((type & 0x0F) != 0x00) {
            pic_state.offset_tbl[cnt++] = pos;
        }
    }
    pic_state.total = cnt;  // 实际有效图片数
    pic_state.index = 0;
    pic_state.active = true;

    pic_task_start_gif(); 
    // 显示第一张图片
    picture_show_current();
}

void pic_task_init(void)
{
    // 预留：初始化图片解码相关全局资源（当前为空）
}

void pic_task_start_gif(void)
{
    if (s_gif_task_handle == NULL) {
        xTaskCreate(gif_task, "gif_task", 8192, NULL, 2, &s_gif_task_handle);
        ESP_LOGI(TAG, "GIF task created");
    }
}

void pic_task_stop_gif(void)
{
    if (s_gif_task_handle != NULL) {
        vTaskDelete(s_gif_task_handle);
        s_gif_task_handle = NULL;
        ESP_LOGI(TAG, "GIF task deleted");
    }
}