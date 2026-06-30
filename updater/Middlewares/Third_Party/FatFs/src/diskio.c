/*-----------------------------------------------------------------------*/
/* Low level disk I/O glue, dispatches to linked Diskio_drv table.       */
/* Ported to FatFs R0.16 (DWORD).                                        */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "ff_gen_drv.h"

extern DiskDrv_t g_diskDrv;

/**
  * @brief Gets disk status.
  * @param pdrv Physical drive number (0..).
  * @retval DSTATUS Operation status.
  */
DSTATUS disk_status(BYTE pdrv)
{
    return g_diskDrv.ppDrv[pdrv]->disk_status(g_diskDrv.lun[pdrv]);
}

/**
  * @brief Initializes a drive.
  * @param pdrv Physical drive number (0..).
  * @retval DSTATUS Operation status.
  */
DSTATUS disk_initialize(BYTE pdrv)
{
    DSTATUS stat = RES_OK;

    if (0U == g_diskDrv.isInitialized[pdrv])
    {
        stat = g_diskDrv.ppDrv[pdrv]->disk_initialize(g_diskDrv.lun[pdrv]);
        if (0U == stat)
        {
            g_diskDrv.isInitialized[pdrv] = 1U;
        }
    }

    return stat;
}

/**
  * @brief Reads sector(s).
  * @param pdrv Physical drive number (0..).
  * @param buff Data buffer to store read data.
  * @param sector Sector address (LBA).
  * @param count Number of sectors to read (1..128).
  * @retval DRESULT Operation result.
  */
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    return g_diskDrv.ppDrv[pdrv]->disk_read(g_diskDrv.lun[pdrv], buff, sector, count);
}

/**
  * @brief Writes sector(s).
  * @param pdrv Physical drive number (0..).
  * @param buff Data to be written.
  * @param sector Sector address (LBA).
  * @param count Number of sectors to write (1..128).
  * @retval DRESULT Operation result.
  */
#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    return g_diskDrv.ppDrv[pdrv]->disk_write(g_diskDrv.lun[pdrv], buff, sector, count);
}
#endif // FF_FS_READONLY == 0

/**
  * @brief I/O control operation.
  * @param pdrv Physical drive number (0..).
  * @param cmd Control code.
  * @param buff Buffer to send/receive control data.
  * @retval DRESULT Operation result.
  */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    return g_diskDrv.ppDrv[pdrv]->disk_ioctl(g_diskDrv.lun[pdrv], cmd, buff);
}

/**
  * @brief Gets time from RTC. Project has no RTC; fixed timestamp via FF_FS_NORTC
  *        is preferred, but this weak symbol is kept for upstream compatibility.
  * @retval Time in DWORD (0 — see fatfs.c override).
  */
__attribute__((weak)) DWORD get_fattime(void)
{
    return 0UL;
}
