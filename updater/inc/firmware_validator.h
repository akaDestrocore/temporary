/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file    firmware_validator.h
 * @brief   Updater bileşeni için firmware dosyası doğrulaması.
 *
 * @author          destrocore
 * @date            2026
 * 
 * @details 
 *  firmwareValidator_run() çağrılmadan önce firmwareValidator_setConfig()
 *  bir kez çağrılmalıdır.
 *  firmwareValidator_run() çağrıldığında dosya konumu 0. byte üzerinde
 *  olmalıdır. Doğrulama işleminden sonra dosya açık kalır.
 */
#ifndef FIRMWARE_VALIDATOR_H
#define FIRMWARE_VALIDATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include "storage_api.h"
#include "image.h"
#include "updater_config.h"
#include "stm32f407xx.h"


/* ============================================================
 * Çalışma zamanı yapılandırması
 * ============================================================*/

/**
 * @brief Doğrulayıcının geçerli bir imaj olarak kabul edeceği sınırlar.
 */
typedef struct {
    uint32_t app_flash_base;        // Uygulama flash bölgesinin ilk byte'ı
    uint32_t app_flash_max_size;    // Byte cinsinden izin verilen maksimum imaj data_size değeri
} FirmwareValidator_Config_t;

/* ============================================================
 * Doğrulama sonuç kodları
 * ============================================================*/
typedef enum {
    FIRMWARE_VALIDATOR_RESULT_OK             =  0,
    FIRMWARE_VALIDATOR_RESULT_BAD_MAGIC      = -1,
    FIRMWARE_VALIDATOR_RESULT_BAD_TYPE       = -2,
    FIRMWARE_VALIDATOR_RESULT_BAD_SIZE       = -3,
    FIRMWARE_VALIDATOR_RESULT_BAD_CRC        = -4,
    FIRMWARE_VALIDATOR_RESULT_READ_ERROR     = -5,
} FirmwareValidator_Result_e;

/**
 * @brief İmaj başlığından çıkarılan veriler.
 */
typedef struct {
    uint32_t data_size;
    uint32_t stored_crc;
    uint32_t computed_crc;
    uint8_t major_ver;
    uint8_t minor_ver;
    uint8_t patch_ver;
    char git_sha[16];
} FirmwareValidator_Info_t;

/**
  * @brief `firmwareValidator_run()` tarafından kullanılan runtime yapılandırmasını saklar.
  * @param pCfg Doldurulmuş bir FirmwareValidator_Config_t yapısına işaretçi.
  * @retval None
  */
void firmwareValidator_setConfig(const FirmwareValidator_Config_t *pCfg);

/**
  * @brief pBackend üzerinden şu anda açık olan firmware dosyasını doğrular.
  * @param pBackend Aktif depolama backend'ine işaretçi.
  * @param pInfo Çıkış verisi; yalnızca _OK dönüşünde doldurulur.
  * @retval FIRMWARE_VALIDATOR_RESULT_OK veya negatif bir hata kodu.
  */
FirmwareValidator_Result_e firmwareValidator_run(const StorageApi_Backend_t *pBackend, FirmwareValidator_Info_t *pInfo);

#ifdef __cplusplus
}
#endif

#endif /* FIRMWARE_VALIDATOR_H */