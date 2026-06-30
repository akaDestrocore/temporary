/**
  ******************************************************************************
  * @file    ff_gen_drv.h
  * @author  MCD Application Team
  * @brief   Header for ff_gen_drv.c module.
  *****************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics. All rights reserved.
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                       opensource.org/licenses/BSD-3-Clause
  ******************************************************************************
  */

#ifndef FF_GEN_DRV_H
#define FF_GEN_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "diskio.h"
#include "ff.h"
#include <stdint.h>

// ============================================================
// Public types
// ============================================================

/**
  * @brief Disk IO driver function pointer table.
  */
typedef struct
{
    DSTATUS (*disk_initialize)(BYTE pDrv);
    DSTATUS (*disk_status)(BYTE pDrv);
    DRESULT (*disk_read)(BYTE pDrv, BYTE *pBuff, DWORD sector, UINT count);
#if FF_FS_READONLY == 0
    DRESULT (*disk_write)(BYTE pDrv, const BYTE *pBuff, DWORD sector, UINT count);
#endif // FF_FS_READONLY == 0
    DRESULT (*disk_ioctl)(BYTE pDrv, BYTE cmd, void *pBuff);
} DiskioDrv_t;

/**
  * @brief Global table of linked disk IO drivers (one per logical volume).
  */
typedef struct
{
    uint8_t          isInitialized[FF_VOLUMES];
    const DiskioDrv_t *ppDrv[FF_VOLUMES];
    uint8_t          lun[FF_VOLUMES];
    volatile uint8_t nbr;
} DiskDrv_t;

// ============================================================
// Public API
// ============================================================

/**
  * @brief Links a compatible diskio driver and returns the assigned logical path.
  * @param pDrv Pointer to the disk IO driver structure.
  * @param pPath Pointer to the logical drive path buffer (min. 4 bytes).
  * @retval 0 on success, 1 if no free volume slot is available.
  */
uint8_t fatfsGenDrv_linkDriver(const DiskioDrv_t *pDrv, char *pPath);

/**
  * @brief Links a compatible diskio driver with an explicit LUN.
  * @param pDrv Pointer to the disk IO driver structure.
  * @param pPath Pointer to the logical drive path buffer (min. 4 bytes).
  * @param lun Logical unit number (multi-LUN devices, e.g. USB MSC).
  * @retval 0 on success, 1 if no free volume slot is available.
  */
uint8_t fatfsGenDrv_linkDriverEx(const DiskioDrv_t *pDrv, char *pPath, BYTE lun);

/**
  * @brief Unlinks a diskio driver bound to the given logical drive path.
  * @param pPath Pointer to the logical drive path.
  * @retval 0 on success, 1 on error.
  */
uint8_t fatfsGenDrv_unlinkDriver(char *pPath);

/**
  * @brief Unlinks a diskio driver with an explicit LUN.
  * @param pPath Pointer to the logical drive path.
  * @param lun Logical unit number.
  * @retval 0 on success, 1 on error.
  */
uint8_t fatfsGenDrv_unlinkDriverEx(char *pPath, BYTE lun);

/**
  * @brief Returns the number of currently linked drivers.
  * @retval Number of attached drivers.
  */
uint8_t fatfsGenDrv_getAttachedDriversNbr(void);

#ifdef __cplusplus
}
#endif

#endif // FF_GEN_DRV_H
