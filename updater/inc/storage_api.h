/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   storage_api.h
 * @brief  Updater bileşeni için backend'den bağımsız depolama arayüzü.
 *
 * @author          destrocore
 * @date            2026
 * 
 * @details
 *  Updater yalnızca bu fonksiyonları çağırır; tüm USB / dosya sistemi
 *  ayrıntıları bu arayüzün arkasında gizlenmiştir. Linkleme sırasında
 *  tam olarak bir storageApi_getBackend() fonksiyonunun somut yerine 
 *  getirilmiş versiyonu sağlar:
 *
 *    `storage_ch376_uart.c` - CH376 UART tabanlı USB-host çipi
 *    `storage_usb_otg.c` - STM32F4 dahili USB OTG FS + FatFS
 *
 *  Yaşam döngüsü:
 *    storage_init() -> storage_waitReady() -> storage_fileOpen() ->
 *    storage_fileRead() -> storage_fileClose() -> storage_deinit()
 *
 *  Tüm fonksiyonlar başarı durumunda StorageApi_Status_OK (0x0),
 *  hata durumunda ise negatif bir StorageApi_Status_e kodu döndürür.
 */

#ifndef STORAGE_API_H
#define STORAGE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Durum kodları
 * ============================================================*/
typedef enum {
    STORAGE_STATUS_OK               = 0U,
    STORAGE_STATUS_NOT_READY        = -1,
    STORAGE_STATUS_FILE_NOT_FOUND   = -2,
    STORAGE_STATUS_READ_ERROR       = -3,
    STORAGE_STATUS_TIMEOUT          = -4,
    STORAGE_STATUS_INVALID_PARAM    = -5
} StorageApi_Status_e;

/* ============================================================
 * Backend fonksiyon işaretçisi tablosu `storage_ch376_uart.c` 
 * veya `storage_usb_otg.c` tarafından doldurulur.
 * ============================================================*/
typedef struct {
    // Donanımı başlatır
    StorageApi_Status_e (*init)(void);

    // Bir USB depolama cihazının mount edilip edilmediğini yoklar
    StorageApi_Status_e (*isReady)(void);

    // Null sonlandırmalı yol adresi, örneğin "/FIRMWARE.BIN"
    StorageApi_Status_e (*fileOpen)(const char *pPath);

    // Açık dosyadan buffSize byte'a kadar veri okur
    StorageApi_Status_e (*fileRead)(uint8_t *pBuff, uint32_t buffSize, uint32_t *pBytesRead);

    // Şu anda açık olan dosyayı kapatır
    StorageApi_Status_e (*fileClose)(void);

    // Donanımı devre dışı bırakır ve kaynakları serbest bırakır
    void (*deinit)(void);
} StorageApi_Backend_t;


/**
 * @brief  Derleme zamanında seçilen backend tablosuna işaretçi döndürür.
 * @retval Tam olarak doldurulmuş bir StorageApi_Backend_t işaretçisi (asla NULL olmamalı)
 */
const StorageApi_Backend_t *storageApi_getBackend(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_API_H */