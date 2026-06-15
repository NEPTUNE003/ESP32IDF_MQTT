#ifndef FF_UTILS_H
#define FF_UTILS_H

#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将簇号转换为扇区地址
 * @param fs   FATFS 文件系统对象
 * @param clst 簇号（>=2）
 * @return 对应的扇区号，失败返回0
 */
LBA_t ff_cluster_to_sector(FATFS* fs, DWORD clst);

/**
 * @brief 将目录指针移动到指定偏移（目录项序号）
 * @param dp   目录对象
 * @param ofs  偏移量（字节，必须是32的倍数，即目录项序号 * 32）
 * @return FR_OK 成功，其他错误码
 */
FRESULT ff_dir_seek_offset(FF_DIR* dp, DWORD ofs);

/**
 * @brief 统计目录下符合指定文件类型（大类）的文件数量
 * @param path     目录路径，如 "0:/MUSIC"
 * @param type_mask 文件类型掩码，例如：
 *                  - 0x40 表示音乐文件（T_WAV, T_MP3等大类）
 *                  - 0x50 表示图片文件（T_BMP, T_JPG等大类）
 *                  - 0x00 统计所有文件（不含子目录）
 * @return 文件数量，失败返回0
 */
uint16_t ff_get_file_count(const char* path, uint8_t type_mask);

#ifdef __cplusplus
}
#endif

#endif