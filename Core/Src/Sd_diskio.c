/*
 * SD Card Diskio Driver for FATFS
 * Links SD SPI functions to FATFS
 */

#include "../../Middlewares/Third_Party/FatFs/src/ff_gen_drv.h"
#include "Sd_spi.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/* _gettimeofday stub for time() / localtime() support */
int _gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (tv) {
        /* Use HAL_GetTick() as time base (ms since boot) */
        tv->tv_sec = HAL_GetTick() / 1000;
        tv->tv_usec = (HAL_GetTick() % 1000) * 1000;
    }
    return 0;
}

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* ============================= */
/* DISKIO FUNCTIONS             */
/* ============================= */

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE pdrv)
{
    printf("[DISKIO] SD_initialize(pdrv=%u) called by FatFs\r\n", pdrv);
    Stat = STA_NOINIT;

    if (sd_init() == 0)
    {
        Stat &= ~STA_NOINIT;
        printf("[DISKIO] [OK] SD_initialize done, Stat=0x%02X\r\n", Stat);
    }
    else
    {
        printf("[DISKIO] [FAIL] SD_initialize failed\r\n");
    }

    return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE pdrv)
{
    printf("[DISKIO] SD_status(pdrv=%u) => Stat=0x%02X\r\n", pdrv, Stat);
    return Stat;
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (Stat & STA_NOINIT) {
        printf("[DISKIO] SD_read(pdrv=%u, sector=%lu, count=%u) => RES_NOTRDY\r\n", pdrv, (unsigned long)sector, count);
        return RES_NOTRDY;
    }

    printf("[DISKIO] SD_read(pdrv=%u, sector=%lu, count=%u) ... ", pdrv, (unsigned long)sector, count);

    if (count == 1)
    {
        if (sd_read_block(buff, sector) == 0) {
            printf("OK (1 block)\r\n");
            return RES_OK;
        }
    }
    else
    {
        if (sd_read_blocks(buff, sector, count) == 0) {
            printf("OK (%u blocks)\r\n", count);
            return RES_OK;
        }
    }

    printf("ERROR\r\n");
    return RES_ERROR;
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT SD_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (Stat & STA_NOINIT) {
        printf("[DISKIO] SD_write(pdrv=%u, sector=%lu, count=%u) => RES_NOTRDY\r\n", pdrv, (unsigned long)sector, count);
        return RES_NOTRDY;
    }

    printf("[DISKIO] SD_write(pdrv=%u, sector=%lu, count=%u) ... ", pdrv, (unsigned long)sector, count);

    // NOTE: Always use sd_write_blocks (CMD25 multi-block) even for count==1.
    // CMD24 single-block write was found to accept data silently without persisting
    // on some SD cards in SPI mode.
    if (sd_write_blocks(buff, sector, count) == 0) {
        printf("OK (%u blocks)\r\n", count);
        return RES_OK;
    }

    printf("ERROR\r\n");
    return RES_ERROR;
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;

    if (Stat & STA_NOINIT)
        return RES_NOTRDY;

    switch (cmd)
    {
        case CTRL_SYNC:
            printf("[DISKIO] SD_ioctl(CTRL_SYNC)\r\n");
            res = RES_OK;
            break;

        case GET_SECTOR_COUNT:
            *(DWORD*)buff = sd_get_sector_count();
            printf("[DISKIO] SD_ioctl(GET_SECTOR_COUNT) => %lu sectors (~%lu MB)\r\n",
                   (unsigned long)*(DWORD*)buff, (unsigned long)(*(DWORD*)buff / 2048));
            res = RES_OK;
            break;

        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            printf("[DISKIO] SD_ioctl(GET_SECTOR_SIZE) => 512\r\n");
            res = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 8;  // 4KB erase block
            printf("[DISKIO] SD_ioctl(GET_BLOCK_SIZE) => 8\r\n");
            res = RES_OK;
            break;

        default:
            printf("[DISKIO] SD_ioctl(cmd=%u) => RES_PARERR\r\n", cmd);
            res = RES_PARERR;
    }

    return res;
}
#endif /* _USE_IOCTL == 1 */

/**
   * @brief  Gets current time for file timestamps
   * @retval DWORD: Packed date/time
   */
__weak DWORD get_fattime(void)
{
    time_t current_time;
    struct tm *time_info;
    DWORD fattime = 0;

    /* Get current time */
    current_time = time(NULL);
    time_info = localtime(&current_time);

    /* Convert to FAT format: YYYYMMDDhhmmss/2 */
    if (time_info) {
        fattime = ((DWORD)(time_info->tm_year - 80) << 25)  /* Year */
                 | ((DWORD)(time_info->tm_mon + 1) << 21)   /* Month */
                 | ((DWORD)time_info->tm_mday << 16)       /* Day */
                 | ((DWORD)time_info->tm_hour << 11)       /* Hour */
                 | ((DWORD)time_info->tm_min << 5)        /* Minute */
                 | ((DWORD)time_info->tm_sec >> 1);       /* Second / 2 */
    } else {
        /* Fallback to fixed date if time is not available */
        fattime = ((DWORD)(2025 - 1980) << 25)  /* Year */
                 | ((DWORD)1 << 21)              /* Month */
                 | ((DWORD)1 << 16)              /* Day */
                 | ((DWORD)0 << 11)              /* Hour */
                 | ((DWORD)0 << 5)               /* Minute */
                 | ((DWORD)0 >> 1);              /* Second / 2 */
    }

    return fattime;
}

/* ============================= */
/* DISKIO DRIVER STRUCTURE      */
/* ============================= */

const Diskio_drvTypeDef SD_Driver =
{
    SD_initialize,
    SD_status,
    SD_read,
#if _USE_WRITE == 1
    SD_write,
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
    SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};
