/**
 * @file   test_firmware_validator.c
 * @brief  updater/src/firmware_validator.c için host tarafında çalışan birim testleri.
 */

#include "unity.h"

#include <string.h>
#include <stdint.h>
#include "firmware_validator.h"
#include "memory_setup.h"
#include "updater_config.h"
#include "image.h"

/* =========================================================================
 * Özel sabitler
 * ========================================================================= */
#define TEST_DATA_SIZE      UPDATER_CHUNK_SIZE
#define TEST_FILL_BYTE      0xA5U
#define TEST_MAJOR_VER      1U
#define TEST_MINOR_VER      2U
#define TEST_PATCH_VER      3U
#define TEST_GIT_SHA        "ef3b84cc"

static uint8_t gImageBuff[2560];

/* =========================================================================
 * Özel yardımcı fonksiyonlar
 * ========================================================================= */
static uint32_t computeCRC_fake(uint8_t fillByte) {
    return ((uint32_t)fillByte) | ((uint32_t)fillByte <<  8U) 
            | ((uint32_t)fillByte << 16U) | ((uint32_t)fillByte << 24U);
}

/* =========================================================================
 * setUp / tearDown
 * ========================================================================= */
void setUp(void) {
    
    virtual_mem_set_all();

    (void)memset(gImageBuff, 0, sizeof(gImageBuff));

    FirmwareValidator_Config_t cfg;
    cfg.app_flash_base = APP_FLASH_BASE;
    cfg.app_flash_max_size = APP_FLASH_MAX_SIZE;
    firmwareValidator_setConfig(&cfg);
}

void tearDown(void) {
    // Hiçbir şey yapmama
}

/* =========================================================================
 * Testler
 * ========================================================================= */
/**
  * @brief Mutlu senaryo: Doğru magic, tür, boyut ve CRC değerlerine sahip geçerli bir imaj
  */
void test_firmwareValidator_validImage_returnsOk(void) {

    (void)memset(gImageBuff, 0U, (size_t)(TEST_DATA_SIZE + IMAGE_HDR_SIZE));

    // Hazırlık
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->image_magic = IMAGE_MAGIC_APP;
    pHdr->image_hdr_version = IMAGE_HDR_VERSION;
    pHdr->image_type = IMAGE_TYPE_APP;
    pHdr->version_major = TEST_MAJOR_VER;
    pHdr->version_minor = TEST_MINOR_VER;
    pHdr->version_patch = TEST_PATCH_VER;
    pHdr->crc = computeCRC_fake(TEST_FILL_BYTE);
    pHdr->data_size = TEST_DATA_SIZE;
    (void)memcpy(pHdr->git_sha, TEST_GIT_SHA, sizeof(TEST_GIT_SHA));

    uint8_t *pData = gImageBuff + IMAGE_HDR_SIZE;
    memset(pData, (int)TEST_FILL_BYTE, sizeof(gImageBuff));

    
}