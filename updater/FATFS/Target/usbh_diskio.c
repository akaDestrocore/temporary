/**
  ******************************************************************************
  * @file    usbh_diskio.c
  * @brief   USB Host disk I/O driver, ported to FatFs R0.16 (DWORD).
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics. All rights reserved.
  ******************************************************************************
  */

#include "ff_gen_drv.h"
#include "usbh_diskio.h"

#define USB_DEFAULT_BLOCK_SIZE 512U

extern USBH_HandleTypeDef hUSB_Host;

static DSTATUS usbh_initialize(BYTE lun);
static DSTATUS usbh_status(BYTE lun);
static DRESULT usbh_read(BYTE lun, BYTE *pBuff, DWORD sector, UINT count);
#if FF_FS_READONLY == 0
static DRESULT usbh_write(BYTE lun, const BYTE *pBuff, DWORD sector, UINT count);
#endif // FF_FS_READONLY == 0
static DRESULT usbh_ioctl(BYTE lun, BYTE cmd, void *pBuff);

const DiskioDrv_t g_usbhDriver =
{
    usbh_initialize,
    usbh_status,
    usbh_read,
#if FF_FS_READONLY == 0
    usbh_write,
#endif // FF_FS_READONLY == 0
    usbh_ioctl,
};

/**
  * @brief Initializes a drive. USB host stack must already be initialized
  *        by the application before this is called.
  * @param lun Logical unit id.
  * @retval DSTATUS Operation status.
  */
static DSTATUS usbh_initialize(BYTE lun)
{
    (void)lun;
    return RES_OK;
}

/**
  * @brief Gets disk status.
  * @param lun Logical unit id.
  * @retval DSTATUS Operation status.
  */
static DSTATUS usbh_status(BYTE lun)
{
    DSTATUS stat;

    if (1U == USBH_MSC_UnitIsReady(&hUSB_Host, lun))
    {
        stat = RES_OK;
    }
    else
    {
        stat = RES_ERROR;
    }

    return stat;
}

/**
  * @brief Reads sector(s).
  * @param lun Logical unit id.
  * @param pBuff Data buffer to store read data.
  * @param sector Sector address (LBA).
  * @param count Number of sectors to read (1..128).
  * @retval DRESULT Operation result.
  */
static DRESULT usbh_read(BYTE lun, BYTE *pBuff, DWORD sector, UINT count)
{
    DRESULT res = RES_ERROR;
    MSC_LUNTypeDef info;

    if (USBH_OK == USBH_MSC_Read(&hUSB_Host, lun, (uint32_t)sector, pBuff, count))
    {
        res = RES_OK;
    }
    else
    {
        (void)USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info);

        switch (info.sense.asc)
        {
            case SCSI_ASC_LOGICAL_UNIT_NOT_READY:
            case SCSI_ASC_MEDIUM_NOT_PRESENT:
            case SCSI_ASC_NOT_READY_TO_READY_CHANGE:
            {
                USBH_ErrLog("USB Disk is not ready!");
                res = RES_NOTRDY;
                break;
            }
            default:
            {
                res = RES_ERROR;
                break;
            }
        }
    }

    return res;
}

/**
  * @brief Writes sector(s).
  * @param lun Logical unit id.
  * @param pBuff Data to be written.
  * @param sector Sector address (LBA).
  * @param count Number of sectors to write (1..128).
  * @retval DRESULT Operation result.
  */
#if FF_FS_READONLY == 0
static DRESULT usbh_write(BYTE lun, const BYTE *pBuff, DWORD sector, UINT count)
{
    DRESULT res = RES_ERROR;
    MSC_LUNTypeDef info;

    if (USBH_OK == USBH_MSC_Write(&hUSB_Host, lun, (uint32_t)sector, (BYTE *)pBuff, count))
    {
        res = RES_OK;
    }
    else
    {
        (void)USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info);

        switch (info.sense.asc)
        {
            case SCSI_ASC_WRITE_PROTECTED:
            {
                USBH_ErrLog("USB Disk is write protected!");
                res = RES_WRPRT;
                break;
            }
            case SCSI_ASC_LOGICAL_UNIT_NOT_READY:
            case SCSI_ASC_MEDIUM_NOT_PRESENT:
            case SCSI_ASC_NOT_READY_TO_READY_CHANGE:
            {
                USBH_ErrLog("USB Disk is not ready!");
                res = RES_NOTRDY;
                break;
            }
            default:
            {
                res = RES_ERROR;
                break;
            }
        }
    }

    return res;
}
#endif // FF_FS_READONLY == 0

/**
  * @brief I/O control operation.
  * @param lun Logical unit id.
  * @param cmd Control code.
  * @param pBuff Buffer to send/receive control data.
  * @retval DRESULT Operation result.
  */
static DRESULT usbh_ioctl(BYTE lun, BYTE cmd, void *pBuff)
{
    DRESULT res = RES_ERROR;
    MSC_LUNTypeDef info;

    switch (cmd)
    {
        case CTRL_SYNC:
        {
            res = RES_OK;
            break;
        }
        case GET_SECTOR_COUNT:
        {
            if (USBH_OK == USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info))
            {
                *(DWORD *)pBuff = (DWORD)info.capacity.block_nbr;
                res = RES_OK;
            }
            break;
        }
        case GET_SECTOR_SIZE:
        {
            if (USBH_OK == USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info))
            {
                *(WORD *)pBuff = info.capacity.block_size;
                res = RES_OK;
            }
            break;
        }
        case GET_BLOCK_SIZE:
        {
            if (USBH_OK == USBH_MSC_GetLUNInfo(&hUSB_Host, lun, &info))
            {
                *(DWORD *)pBuff = info.capacity.block_size / USB_DEFAULT_BLOCK_SIZE;
                res = RES_OK;
            }
            break;
        }
        default:
        {
            res = RES_PARERR;
            break;
        }
    }

    return res;
}
