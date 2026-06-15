#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "fonts.h"
#include "exfuns.h"

#include "lcd.h"
#include "text.h"
#include "xl9555.h"
#include "es8388.h"
#include "myi2s.h"
#include "audioplay.h"
#include "wavplay.h"
#include "spi_sd.h"
#include "audio_common.h"

#include "music_task.h"

static const char *TAG = "MUSIC_TASK";

static TaskHandle_t s_music_task_handle = NULL;
static bool s_is_playing = false;

volatile bool g_music_next = false;
volatile bool g_music_prev = false;
volatile bool g_music_playpause = false;
volatile bool g_music_exit_req = false;

// 音乐播放主任务
static void music_task_func(void *pvParameters)
{
    ESP_LOGI(TAG, "Music task started");

    // 打开喇叭
    xl9555_pin_write(SPK_EN_IO, 0);

    // 启动音频播放（该函数会阻塞，直到收到退出命令或用户按返回键）
    audio_play();   // audio_play 内部已增加退出标志检查

    // 播放结束，清理资源
    music_exit();

    vTaskDelete(NULL);
}

void __attribute__((used)) music_enter(void)
{
    if (s_music_task_handle != NULL) {
        ESP_LOGW(TAG, "Music already playing");
        return;
    }

    // 清屏显示加载提示
    spilcd_clear(WHITE);
    text_show_string(30, 30, 240, 16, "Music Player Starting...", 16, 0, BLUE);

    // 初始化 FATFS 扩展和字库
    if (exfuns_init() != 0) {
        text_show_string(30, 130, 240, 16, "exfuns Init Failed!", 16, 0, RED);
        vTaskDelay(pdMS_TO_TICKS(1000));
        spilcd_clear(WHITE);
        return;
    }
    if (fonts_init() != 0) {
        text_show_string(30, 130, 240, 16, "Fonts Init Failed!", 16, 0, RED);
        vTaskDelay(pdMS_TO_TICKS(1000));
        spilcd_clear(WHITE);
        return;
    }

    // 初始化 SD 卡（挂载文件系统）
    if (sd_spi_init() != ESP_OK) {
        text_show_string(30, 130, 240, 16, "SD Card Error!", 16, 0, RED);
        vTaskDelay(pdMS_TO_TICKS(1000));
        spilcd_clear(WHITE);
        return;
    }

    // 初始化 I2S 和 ES8388
    if (myi2s_init() != ESP_OK) {
        text_show_string(30, 130, 240, 16, "I2S Init Failed!", 16, 0, RED);
        vTaskDelay(pdMS_TO_TICKS(1000));
        spilcd_clear(WHITE);
        return;
    }
    if (es8388_init() != 0) {
        text_show_string(30, 130, 240, 16, "ES8388 Error!", 16, 0, RED);
        vTaskDelay(pdMS_TO_TICKS(1000));
        spilcd_clear(WHITE);
        return;
    }

    // ES8388 输出配置
    es8388_adda_cfg(1, 0);          // 打开 DAC，关闭 ADC
    es8388_input_cfg(0);            // 关闭录音输入
    es8388_output_cfg(1, 1);        // 打开喇叭通道和耳机通道
    es8388_hpvol_set(20);           // 设置耳机音量
    es8388_spkvol_set(20);          // 设置喇叭音量
    xl9555_pin_write(SPK_EN_IO, 0); // 确保喇叭电源开启

    // 复位控制标志
    g_music_next = g_music_prev = g_music_playpause = g_music_exit_req = false;

    // 创建音乐播放任务
    xTaskCreate(music_task_func, "music_task", 8192, NULL, 3, &s_music_task_handle);
    s_is_playing = true;
}

void __attribute__((used)) music_exit(void)
{
    static bool is_exiting = false;   // 防重入标志

    // 如果已经在退出过程中，直接返回
    if (is_exiting) {
        ESP_LOGW(TAG, "music_exit already in progress, skip");
        return;
    }

    is_exiting = true;
    
    if (s_music_task_handle) {
        // 发送退出请求
        g_music_exit_req = true;
        // 等待任务结束
        vTaskDelay(pdMS_TO_TICKS(200));
        if (s_music_task_handle) {
            vTaskDelete(s_music_task_handle);
            s_music_task_handle = NULL;
        }
    }

    // 停止音频输出
    i2s_trx_stop();
    i2s_deinit();
    es8388_deinit();
    xl9555_pin_write(SPK_EN_IO, 1);   // 关闭喇叭

    s_is_playing = false;
    // 复位所有控制标志
    g_music_next = g_music_prev = g_music_playpause = g_music_exit_req = false;

    ESP_LOGI(TAG, "Music exited");
}

// 以下函数供 menu 调用，直接设置全局标志
void music_next(void)
{
    g_music_next = true;
}

void music_prev(void)
{
    g_music_prev = true;
}

void music_playpause(void)
{
    g_music_playpause = true;
}

bool music_is_playing(void)
{
    return s_is_playing;
}

void __attribute__((used)) music_update(void)
{
}