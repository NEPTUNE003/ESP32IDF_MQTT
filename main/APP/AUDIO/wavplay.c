#include "wavplay.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_common.h"

__wavctrl wavctrl;                          /* WAV 音频文件解码参数结构体 */
UINT bytes_write = 0;                       /* 写一次 I2S 大小（已无用，保留兼容） */
volatile long long int i2s_table_size = 0;  /* 已发送音频数据总大小 */
esp_err_t i2s_play_end = ESP_FAIL;          /* 播放结束标志位 */
esp_err_t i2s_play_next_prev = ESP_FAIL;    /* 下一首/上一首标志位 */
FSIZE_t file_read_pos = 0;                  /* 暂停时记录文件位置（已无用，保留） */


/**
 * @brief       WAV 解析初始化
 * @param       fname : 文件路径+文件名
 * @param       wavx  : 信息存放结构体指针
 * @retval      0 = 成功, 1 = 打开文件失败, 2 = 非 WAV 文件, 3 = DATA 区域未找到
 */
uint8_t wav_decode_init(uint8_t *fname, __wavctrl *wavx)
{
    FIL *ftemp;
    uint8_t *buf; 
    uint32_t br = 0;
    uint8_t res = 0;

    ChunkRIFF *riff;
    ChunkFMT *fmt;
    ChunkFACT *fact;
    ChunkDATA *data;
    
    ftemp = (FIL*)malloc(sizeof(FIL));
    buf = malloc(512);
    
    if (ftemp && buf)
    {
        res = f_open(ftemp, (TCHAR*)fname, FA_READ);
        if (res == FR_OK)
        {
            f_read(ftemp, buf, 512, (UINT *)&br);
            riff = (ChunkRIFF *)buf;
            if (riff->Format == 0x45564157)         /* 是 WAV 文件 */
            {
                fmt = (ChunkFMT *)(buf + 12);
                fact = (ChunkFACT *)(buf + 12 + 8 + fmt->ChunkSize);
                if (fact->ChunkID == 0x74636166 || fact->ChunkID == 0x5453494C)
                    wavx->datastart = 12 + 8 + fmt->ChunkSize + 8 + fact->ChunkSize;
                else
                    wavx->datastart = 12 + 8 + fmt->ChunkSize;
                
                data = (ChunkDATA *)(buf + wavx->datastart);
                if (data->ChunkID == 0x61746164)    /* 解析成功 */
                {
                    wavx->audioformat = fmt->AudioFormat;
                    wavx->nchannels = fmt->NumOfChannels;
                    wavx->samplerate = fmt->SampleRate;
                    wavx->bitrate = fmt->ByteRate * 8;
                    wavx->blockalign = fmt->BlockAlign;
                    wavx->bps = fmt->BitsPerSample;
                    wavx->datasize = data->ChunkSize;
                    wavx->datastart = wavx->datastart + 8;
                     
                    printf("wavx->audioformat:%d\r\n", wavx->audioformat);
                    printf("wavx->nchannels:%d\r\n", wavx->nchannels);
                    printf("wavx->samplerate:%ld\r\n", wavx->samplerate);
                    printf("wavx->bitrate:%ld\r\n", wavx->bitrate);
                    printf("wavx->blockalign:%d\r\n", wavx->blockalign);
                    printf("wavx->bps:%d\r\n", wavx->bps);
                    printf("wavx->datasize:%ld\r\n", wavx->datasize);
                    printf("wavx->datastart:%ld\r\n", wavx->datastart);  
                }
                else res = 3;
            }
            else res = 2;
        }
        else res = 1;
    }
    
    f_close(ftemp);
    free(ftemp);
    free(buf); 
    return res;
}

/**
 * @brief       获取当前播放时间（基于文件指针位置）
 * @param       fx    : 文件指针
 * @param       wavx  : wav 播放控制器
 */
void wav_get_curtime(FIL *fx, __wavctrl *wavx)
{
    long long fpos = 0;
    wavx->totsec = wavx->datasize / (wavx->bitrate / 8);
    fpos = fx->fptr - wavx->datastart;
    wavx->cursec = fpos * wavx->totsec / wavx->datasize;
}

/**
 * @brief       播放指定的 WAV 文件（阻塞，内部完成音频数据发送）
 * @param       fname : 文件路径+文件名
 * @retval      KEY0_PRES = 下一首, KEY1_PRES = 上一首, 0xFF = 退出音乐播放
 */
uint8_t wav_play_song(uint8_t *fname)
{
    uint8_t res = 0;

    i2s_play_end = ESP_FAIL;
    i2s_play_next_prev = ESP_FAIL;
    g_audiodev.file = (FIL*)heap_caps_malloc(sizeof(FIL), MALLOC_CAP_DMA);
    g_audiodev.tbuf = heap_caps_malloc(WAV_TX_BUFSIZE, MALLOC_CAP_DMA);

    if (!g_audiodev.file || !g_audiodev.tbuf) {
        return 0xFF;
    }

    memset(g_audiodev.file, 0, sizeof(FIL));
    memset(g_audiodev.tbuf, 0, WAV_TX_BUFSIZE);
    memset(&wavctrl, 0, sizeof(__wavctrl));
    if (wav_decode_init(fname, &wavctrl) != 0) {
        goto cleanup;
    }



    /* 打开音频文件 */
    if (f_open(g_audiodev.file, (TCHAR*)fname, FA_READ) != FR_OK) {
        goto cleanup;
    }

  /* 音频数据发送循环 */
    i2s_table_size = 0;
    while (1) {
        /* 处理外部控制标志 */
        if (g_music_next) {
            g_music_next = false;
            res = KEY0_PRES;
            break;
        }
        if (g_music_prev) {
            g_music_prev = false;
            res = KEY1_PRES;
            break;
        }
        if (g_music_playpause) {
            g_music_playpause = false;
            // 直接切换软件状态
            if ((g_audiodev.status & 0x0F) == 0x03)
                audio_stop();
            else if ((g_audiodev.status & 0x0F) == 0x00)
                audio_start();
        }
        if (g_music_exit_req) {
            music_set_exit_flag();
            vTaskDelay(pdMS_TO_TICKS(100));
            res = 0xFF;
            break;
        }

        // 仅当处于播放状态时才读取和发送数据
        if ((g_audiodev.status & 0x0F) == 0x03) {
            UINT br;
            f_read(g_audiodev.file, g_audiodev.tbuf, WAV_TX_BUFSIZE, &br);
            if (br == 0) {
                res = KEY0_PRES;   // 自然结束，下一首
                break;
            }
            i2s_tx_write(g_audiodev.tbuf, br);
            i2s_table_size += br;

            // 更新时间显示
            wav_get_curtime(g_audiodev.file, &wavctrl);
            audio_msg_show(wavctrl.totsec, wavctrl.cursec, wavctrl.bitrate);
        } else {
            // 暂停状态：只延时，不读文件
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    /* 注意：这里不停止 I2S，以便下一首歌继续使用；退出界面时由 music_exit 负责停止 */
    // audio_stop();

cleanup:
    f_close(g_audiodev.file);
    heap_caps_free(g_audiodev.file);
    heap_caps_free(g_audiodev.tbuf);
    g_audiodev.tbuf = NULL;
    g_audiodev.file = NULL;

    return res;
}