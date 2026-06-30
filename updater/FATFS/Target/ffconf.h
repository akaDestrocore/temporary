/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  *  FatFs - Generic FAT file system module  R0.12c (C)ChaN, 2017
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef _FFCONF
#define _FFCONF 80386

#include "updater.h"
#include "stm32f4xx_hal.h"
#include "usbh_core.h"
#include "usbh_msc.h"

#define hUSB_Host hUsbHostFS

// ---- Function configurations ----
#define FF_FS_READONLY      0
#define FF_FS_MINIMIZE      0
#define FF_USE_FIND         0
#define FF_USE_MKFS         1
#define FF_USE_FASTSEEK     1
#define FF_USE_EXPAND       0
#define FF_USE_CHMOD        0
#define FF_USE_LABEL        0
#define FF_USE_FORWARD      0
#define FF_USE_STRFUNC      2
#define FF_PRINT_LLI        0
#define FF_PRINT_FLOAT      0
#define FF_STRF_ENCODE      0

// ---- Locale / namespace ----
#define FF_CODE_PAGE        850

// Static LFN buffer (no RTOS reentrancy concerns; firmware path is sequential)
#define FF_USE_LFN          1
#define FF_MAX_LFN          255
#define FF_LFN_UNICODE      0
#define FF_LFN_BUF          255
#define FF_SFN_BUF          12
#define FF_FS_RPATH         0
#define FF_PATH_DEPTH       10

// ---- Drive / volume ----
#define FF_VOLUMES          1
#define FF_STR_VOLUME_ID    0
#define FF_VOLUME_STRS      "RAM","NAND","CF","SD1","SD2","USB1","USB2","USB3"
#define FF_MULTI_PARTITION  0
#define FF_MIN_SS           512
#define FF_MAX_SS           512
#define FF_LBA64            0
#define FF_MIN_GPT          0x10000000
#define FF_USE_TRIM         0

// ---- System ----
#define FF_FS_TINY          0
#define FF_FS_EXFAT         0
#define FF_FS_NORTC         0
#define FF_NORTC_MON        6
#define FF_NORTC_MDAY       4
#define FF_NORTC_YEAR       2026
#define FF_FS_CRTIME        0
#define FF_FS_NOFSINFO      0
#define FF_FS_LOCK          2
#define FF_FS_REENTRANT     0
#define FF_FS_TIMEOUT       1000

#endif // _FFCONF
