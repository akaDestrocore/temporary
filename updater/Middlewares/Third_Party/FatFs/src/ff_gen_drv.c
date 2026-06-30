/**
  ******************************************************************************
  * @file    ff_gen_drv.c
  * @author  MCD Application Team
  * @brief   FatFs generic low level driver.
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

#include "ff_gen_drv.h"

// ============================================================
// Module state
// ============================================================
DiskDrv_t g_diskDrv = { { 0 }, { 0 }, { 0 }, 0U };

// ============================================================
// Public API
// ============================================================

/**
  * @brief Links a compatible diskio driver/lun id and increments the number of
  *        active linked drivers.
  * @param pDrv Pointer to the disk IO driver structure.
  * @param pPath Pointer to the logical drive path.
  * @param lun Logical unit id (0 for single-LUN devices).
  * @retval 0 on success, 1 otherwise.
  */
uint8_t fatfsGenDrv_linkDriverEx(const DiskioDrv_t *pDrv, char *pPath, BYTE lun)
{
    uint8_t ret = 1U;
    uint8_t diskNum = 0U;

    if (g_diskDrv.nbr < FF_VOLUMES)
    {
        g_diskDrv.isInitialized[g_diskDrv.nbr] = 0U;
        g_diskDrv.ppDrv[g_diskDrv.nbr] = pDrv;
        g_diskDrv.lun[g_diskDrv.nbr] = lun;
        diskNum = g_diskDrv.nbr;
        g_diskDrv.nbr++;

        pPath[0] = (char)(diskNum + '0');
        pPath[1] = ':';
        pPath[2] = '/';
        pPath[3] = 0;
        ret = 0U;
    }

    return ret;
}

/**
  * @brief Links a compatible diskio driver and increments the number of
  *        active linked drivers.
  * @param pDrv Pointer to the disk IO driver structure.
  * @param pPath Pointer to the logical drive path.
  * @retval 0 on success, 1 otherwise.
  */
uint8_t fatfsGenDrv_linkDriver(const DiskioDrv_t *pDrv, char *pPath)
{
    return fatfsGenDrv_linkDriverEx(pDrv, pPath, 0U);
}

/**
  * @brief Unlinks a diskio driver and decrements the number of active linked
  *        drivers.
  * @param pPath Pointer to the logical drive path.
  * @param lun Not used.
  * @retval 0 on success, 1 otherwise.
  */
uint8_t fatfsGenDrv_unlinkDriverEx(char *pPath, BYTE lun)
{
    uint8_t diskNum = 0U;
    uint8_t ret = 1U;

    (void)lun;

    if (g_diskDrv.nbr >= 1U)
    {
        diskNum = (uint8_t)(pPath[0] - '0');
        if (NULL != g_diskDrv.ppDrv[diskNum])
        {
            g_diskDrv.ppDrv[diskNum] = NULL;
            g_diskDrv.lun[diskNum] = 0U;
            g_diskDrv.nbr--;
            ret = 0U;
        }
    }

    return ret;
}

/**
  * @brief Unlinks a diskio driver and decrements the number of active linked
  *        drivers.
  * @param pPath Pointer to the logical drive path.
  * @retval 0 on success, 1 otherwise.
  */
uint8_t fatfsGenDrv_unlinkDriver(char *pPath)
{
    return fatfsGenDrv_unlinkDriverEx(pPath, 0U);
}

/**
  * @brief Gets number of linked drivers to the FatFs module.
  * @retval Number of attached drivers.
  */
uint8_t fatfsGenDrv_getAttachedDriversNbr(void)
{
    return g_diskDrv.nbr;
}
