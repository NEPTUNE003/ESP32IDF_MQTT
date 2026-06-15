#include "audioplay.h"
#include "audio_common.h"
#include "ff_utils.h"

static bool g_music_exit_flag = false;   // 退出音乐总循环标志
extern QueueHandle_t s_cmd_queue;

__audiodev g_audiodev;          /* 音乐播放控制器 */


void audio_start(void)
{
    g_audiodev.status = 3 << 0;   // 仅设置播放标志，不操作硬件
}

void audio_stop(void)
{
    g_audiodev.status = 0;        // 仅设置暂停标志，不操作硬件
}

uint16_t audio_get_tnum(uint8_t *path)
{
    return ff_get_file_count((const char*)path, 0x40);
}

/**
 * @brief       显示曲目索引
 * @param       index : 当前索引
 * @param       total : 总文件数
 * @retval      无
 */
void audio_index_show(uint16_t index, uint16_t total)
{
    spilcd_show_num(30 + 0, 100, index, 3, 16, RED);
    spilcd_show_char(30 + 24, 100, '/', 16, 0, RED);
    spilcd_show_num(30 + 32, 100, total, 3, 16, RED);
}

/**
 * @brief       显示播放时间,比特率 信息
 * @param       totsec : 音频文件总时间长度
 * @param       cursec : 当前播放时间
 * @param       bitrate: 比特率(位速)
 * @retval      无
 */
void audio_msg_show(uint32_t totsec, uint32_t cursec, uint32_t bitrate)
{
    static uint16_t playtime = 0xFFFF;
    
    if (playtime != cursec)
    {
        playtime = cursec;
        
        spilcd_show_xnum(30, 130, playtime / 60, 2, 16, 0X80, RED);
        spilcd_show_char(30 + 16, 130, ':', 16, 0, RED);
        spilcd_show_xnum(30 + 24, 130, playtime % 60, 2, 16, 0X80, RED);
        spilcd_show_char(30 + 40, 130, '/', 16, 0, RED);
        
        spilcd_show_xnum(30 + 48, 130, totsec / 60, 2, 16, 0X80, RED);
        spilcd_show_char(30 + 64, 130, ':', 16, 0, RED);
        spilcd_show_xnum(30 + 72, 130, totsec % 60, 2, 16, 0X80, RED);
        
        spilcd_show_num(30 + 110, 130, bitrate / 1000, 4, 16, RED);
        spilcd_show_string(30 + 110 + 32 , 130, 200, 16, 16, "Kbps", RED);
    }
}


// 新增函数：设置退出标志
void music_set_exit_flag(void)
{
    g_music_exit_flag = true;
}

void audio_play(void)
{
    uint8_t res;
    FF_DIR wavdir;
    FILINFO *wavfileinfo;
    uint8_t *pname;
    uint16_t totwavnum;
    uint16_t curindex;
    uint8_t key;
    uint32_t temp;
    uint32_t *wavoffsettbl;

    // 检查 /MUSIC 目录是否存在
    while (f_opendir(&wavdir, "0:/MUSIC")) {
        text_show_string(30, 190, 240, 16, "MUSIC文件夹错误!", 16, 0, BLUE);
        vTaskDelay(200);
        spilcd_fill(30, 190, 240, 206, WHITE);
        vTaskDelay(200);
    }

    totwavnum = audio_get_tnum((uint8_t *)"0:/MUSIC");
    while (totwavnum == 0) {
        text_show_string(30, 190, 240, 16, "没有音乐文件!", 16, 0, BLUE);
        vTaskDelay(200);
        spilcd_fill(30, 190, 240, 146, WHITE);
        vTaskDelay(200);
    }

    wavfileinfo = (FILINFO*)malloc(sizeof(FILINFO));
    pname = malloc(255 * 2 + 1);
    wavoffsettbl = malloc(4 * totwavnum);

    while (!wavfileinfo || !pname || !wavoffsettbl) {
        text_show_string(30, 190, 240, 16, "内存分配失败!", 16, 0, BLUE);
        vTaskDelay(200);
        spilcd_fill(30, 190, 240, 146, WHITE);
        vTaskDelay(200);
    }

    res = f_opendir(&wavdir, "0:/MUSIC");
    if (res == FR_OK) {
        curindex = 0;
        while (1) {
            temp = wavdir.dptr;
            res = f_readdir(&wavdir, wavfileinfo);
            if ((res != FR_OK) || (wavfileinfo->fname[0] == 0)) break;
            if ((exfuns_file_type(wavfileinfo->fname) & 0xF0) == 0x40) {
                wavoffsettbl[curindex] = temp;
                curindex++;
            }
        }
    }

    curindex = 0;
    res = f_opendir(&wavdir, (const TCHAR*)"0:/MUSIC");

    while (res == FR_OK && !g_music_exit_flag)   // 增加退出标志检查
    {
        ff_dir_seek_offset(&wavdir, wavoffsettbl[curindex]);
        res = f_readdir(&wavdir, wavfileinfo);
        if ((res != FR_OK) || (wavfileinfo->fname[0] == 0)) break;

        strcpy((char *)pname, "0:/MUSIC/");
        strcat((char *)pname, (const char *)wavfileinfo->fname);
        spilcd_fill(30, 190, spilcddev.width, spilcddev.height, WHITE);
        audio_index_show(curindex + 1, totwavnum);
        text_show_string(30, 190, 300, 16, (char *)wavfileinfo->fname, 16, 0, BLUE);
        key = audio_play_song(pname);

        if (g_music_exit_flag) break;

        if (key == KEY0_PRES) {
            curindex++;
            if (curindex >= totwavnum) curindex = 0;
        } else if (key == KEY1_PRES) {
            if (curindex) curindex--;
            else curindex = totwavnum - 1;
        } else if (key == 0xFF) {
            break;
        }
}

    free(wavfileinfo);
    free(pname);
    free(wavoffsettbl);
}

/**
 * @brief       播放某个音频文件
 * @param       fname : 文件名
 * @retval      按键值
 *   @arg       KEY0_PRES , 下一曲.
 *   @arg       KEY1_PRES , 上一曲.
 *   @arg       其他 , 错误
 */
uint8_t audio_play_song(uint8_t *fname)
{
    uint8_t res;  
    
    res = exfuns_file_type((char *)fname); 

    switch (res)
    {
        case T_WAV:
            res = wav_play_song(fname);
            break;
        case T_MP3:
            /* 自行实现 */
            break;

        default:            /* 其他文件,自动跳转到下一曲 */
            printf("can't play:%s\r\n", fname);
            res = KEY0_PRES;
            break;
    }
    return res;
}
