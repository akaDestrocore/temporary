/*-----------------------------------------------------------------------/
/  Low level disk interface module include file, ported to FatFs R0.16   /
/-----------------------------------------------------------------------*/

#ifndef DISKIO_DEFINED
#define DISKIO_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

#include "integer.h"

// Status of Disk Functions
typedef BYTE DSTATUS;

// Results of Disk Functions
typedef enum
{
    RES_OK = 0,    // 0: Successful
    RES_ERROR,     // 1: R/W error
    RES_WRPRT,     // 2: Write protected
    RES_NOTRDY,    // 3: Not ready
    RES_PARERR     // 4: Invalid parameter
} DRESULT;

// ============================================================
// Prototypes for disk control functions
// ============================================================
DSTATUS disk_initialize(BYTE pdrv);
DSTATUS disk_status(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);
DWORD get_fattime(void);

// Disk Status Bits (DSTATUS)
#define STA_NOINIT      0x01U  // Drive not initialized
#define STA_NODISK      0x02U  // No medium in the drive
#define STA_PROTECT     0x04U  // Write protected

// Command code for disk_ioctl function (generic, used by FatFs)
#define CTRL_SYNC           0U
#define GET_SECTOR_COUNT    1U
#define GET_SECTOR_SIZE     2U
#define GET_BLOCK_SIZE      3U
#define CTRL_TRIM           4U

#ifdef __cplusplus
}
#endif

#endif // DISKIO_DEFINED
