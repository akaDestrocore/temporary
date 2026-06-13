/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   firmware_validator.c
 * @brief  Validates a firmware binary streamed from the StorageApi.
 *
 * @author  destrocore
 * @date    2026
 * 
 */

#include "firmware_validator.h"

/* ============================================================
 * Özel sabitler
 * ============================================================*/
static uint8_t gHeaderBuff[IMAGE_HDR_SIZE];
static uint8_t gReadBuff[UPDATER_CHUNK_SIZE];
static FirmwareValidator_Config_t gConfig;

/* ============================================================
 * Özel prototipler
 * ============================================================*/
static void validator_crcStart(void);
static void validator_crcFeed(const uint8_t *pData, uint32_t len);
static uint32_t validator_crcResult(void);

/* ============================================================
 * Genel API
 * ============================================================*/

 /**
  * @brief pBackend üzerinden şu anda açık olan firmware dosyasını doğrular.
  * @param pBackend Aktif depolama backend'ine işaretçi.
  * @param pInfo Çıkış verisi; yalnızca _OK dönüşünde doldurulur.
  * @retval FIRMWARE_VALIDATOR_RESULT_OK veya negatif bir hata kodu.
  */
FirmwareValidator_Result_e firmwareValidator_run(const StorageApi_Backend_t *pBackend, FirmwareValidator_Info_t *pInfo) {

    if(NULL == pBackend) {
        return FIRMWARE_VALIDATOR_RESULT_READ_ERROR;
    }

    // -- İmaj başlığı okuma ---------------------------------
    uint32_t bytesRead = 0U;
    StorageApi_Status_e status = pBackend->fileRead(gHeaderBuff, (uint32_t)IMAGE_HDR_SIZE, &bytesRead);

    if((STORAGE_STATUS_OK != status) || ((uint32_t)IMAGE_HDR_SIZE != bytesRead)) {
        return FIRMWARE_VALIDATOR_RESULT_READ_ERROR;
    }

    const Image_Hdr_t *pHdr = (const Image_Hdr_t *)gHeaderBuff;

    // -- Magic doğrulaması ---------------------------------
    if (IMAGE_MAGIC_APP != pHdr->image_magic) {
        return FIRMWARE_VALIDATOR_RESULT_BAD_MAGIC;
    }

    // -- Imaj türü kontrolü ---------------------------------
    if (IMAGE_TYPE_APP != pHdr->image_type) {
        return  FIRMWARE_VALIDATOR_RESULT_BAD_TYPE;
    }

    // -- Veri boyutu runtime yapılandırmasıyla karşılaştırma -
    if ((0U == pHdr->data_size) || (pHdr->data_size > gConfig.app_flash_max_size)) {
        return FIRMWARE_VALIDATOR_RESULT_BAD_SIZE;
    }

    // -- CRC aracılığıyla veri parçasını akış halinde gönderme
    validator_crcStart();

    uint32_t remaining = pHdr->data_size;

    while (remaining > 0U) {
        uint32_t toRead = (remaining < (uint32_t)UPDATER_CHUNK_SIZE) ? remaining : (uint32_t)UPDATER_CHUNK_SIZE;

        uint32_t got = 0U;
        status = pBackend->fileRead(gReadBuff, toRead, &got);

        if ((STORAGE_STATUS_OK != status) || (0U == got)) {
            return FIRMWARE_VALIDATOR_RESULT_READ_ERROR;
        }

        validator_crcFeed(gReadBuff, got);
        remaining -= got;
    }

    uint32_t computedCRC = validator_crcResult();

    // -- CRC'lerin karşılaştırılması --------------------------
    if (computedCRC != pHdr->crc) {
        return FIRMWARE_VALIDATOR_RESULT_BAD_CRC;
    }

    // -- Çağıranın bilgilerinin doldurması --------------------
    if (NULL != pInfo) {
        pInfo->data_size = pHdr->data_size;
        pInfo->stored_crc = pHdr->crc;
        pInfo->computed_crc = computedCRC;
        pInfo->major_ver = pHdr->version_major;
        pInfo->minor_ver = pHdr->version_minor;
        pInfo->patch_ver = pHdr->version_patch;
        (void)memcpy(pInfo->git_sha, pHdr->git_sha, sizeof(pInfo->git_sha));
    }

    return FIRMWARE_VALIDATOR_RESULT_OK;
}

/**
  * @brief  Store the runtime configuration used by firmwareValidator_run().
  * @param  pCfg  Pointer to a fully populated FirmwareValidator_Config_t.
  * @retval None
  */
void firmwareValidator_setConfig(const FirmwareValidator_Config_t *pCfg) {
    
    if (NULL == pCfg) {
        return;
    }
    gConfig = *pCfg;
}

/* ================================================================
 * CRC yardımcıları
 * ================================================================*/

 /**
  * @brief  CRC çevre birimi saatini etkinleştirir ve sayacı sıfırlar.
  * @retval None
  */
static void validator_crcStart(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
    (void)RCC->AHB1ENR;
    CRC->CR = CRC_CR_RESET;
}

/**
  * @brief Ham bayt buffer'ı CRC'ye besler.
  * @param pData Veri buffer'ına işaretçi.
  * @param len Bayt cinsinden uzunluk.
  * @retval None
  */
static void validator_crcFeed(const uint8_t *pData, uint32_t len) {
    uint32_t words = len / 4U;
    for (uint32_t i = 0U; i < words; i++) {
        uint32_t word;
        (void)memcpy(&word, &pData[i * 4U], 4U);
        CRC->DR = word;
    }

    uint32_t remainder = len % 4U;
    if (0U != remainder) {
        uint32_t last = 0U;
        uint32_t base = len - remainder;
        for (uint32_t i = 0U; i < remainder; i++) {
            last |= (uint32_t)pData[base + i] << (i * 8U);
        }
        CRC->DR = last;
    }
}

/**
  * @brief Mevcut CRC değerini döndürür.
  * @retval CRC32 sonucu.
  */
static uint32_t validator_crcResult(void) {
    return CRC->DR;
}