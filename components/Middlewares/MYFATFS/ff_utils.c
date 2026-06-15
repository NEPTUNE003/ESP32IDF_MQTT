#include "ff_utils.h"
#include "exfuns.h"   
#include <stdlib.h>
#include <string.h>

LBA_t ff_cluster_to_sector(FATFS* fs, DWORD clst)
{
    clst -= 2;
    if (clst >= fs->n_fatent - 2) return 0;
    return fs->database + (LBA_t)fs->csize * clst;
}

FRESULT ff_dir_seek_offset(FF_DIR* dp, DWORD ofs)
{
    DWORD clst;
    FATFS *fs = dp->obj.fs;

    if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? 0x10000000 : 0x200000) || ofs % 32)
        return FR_INT_ERR;

    dp->dptr = ofs;
    clst = dp->obj.sclust;

    if (clst == 0 && fs->fs_type >= FS_FAT32) {
        clst = (DWORD)fs->dirbase;
        if (FF_FS_EXFAT) dp->obj.stat = 0;
    }

    if (clst == 0) {
        if (ofs / 32 >= fs->n_rootdir) return FR_INT_ERR;
        dp->sect = fs->dirbase;
    } else {
        dp->sect = ff_cluster_to_sector(fs, clst);
        if (dp->sect == 0) return FR_INT_ERR;
    }

    dp->clust = clst;
    dp->sect += ofs / fs->ssize;
    dp->dir = fs->win + (ofs % fs->ssize);

    return FR_OK;
}

uint16_t ff_get_file_count(const char* path, uint8_t type_mask)
{
    FF_DIR dir;
    FILINFO fno;
    uint16_t count = 0;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) return 0;

    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        /* 跳过子目录（仅统计文件） */
        if (fno.fattrib & AM_DIR) continue;

        uint8_t file_type = exfuns_file_type(fno.fname);
        if (type_mask == 0) {
            count++;  // 统计所有文件
        } else {
            /* 检查高4位（大类）是否匹配 */
            if ((file_type & 0xF0) == type_mask) count++;
        }
    }

    f_closedir(&dir);
    return count;
}