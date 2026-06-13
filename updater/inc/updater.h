/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   updater.h
 * @brief  Güncelleyen önyükleyici genel API ve üst seviye yapılandırma.
 *
 * @author  destrocore
 * @date    2026
 * 
 * @details
 *  Her alt modül için struct tipleri ilgili modülün kendi header dosyasında
 *  tanımlanır ve dahil etme yoluyla burada yeniden dışa aktarılır.
 *  Bu nedenle `updater.h` kendi içinde herhangi bir struct gövdesi içermez -
 *  yalnızca Updater_Config_t, hata enum'u ve iki adet genel API prototipi içerir.
 *
 *  USB_VBUS_EN_PORT / USB_VBUS_EN_PIN_BSRR tanımlarını usbh_conf.c dosyasına
 *  sağlayan include zinciri:
 *    usbh_conf.c → usbh_conf.h → updater.h → updater_config.h
 *
 *  Kullanım:
 *      1. Her alt yapılandırma struct'ını içeren bir Updater_Config_t doldurun.
 *      2. `HAL_Init()` ve `rcc_init()` sonrasında `updater_init()` fonksiyonunu bir kez çağırın.
 *      3. `updater_run()` fonksiyonunu çağırın. Başarı durumunda geri dönmez.
 *          Hata durumunda ilgili Updater_Err_e kodunu döndürür.
 */

#ifndef UPDATER_H
#define UPDATER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "updater_config.h"
#include "firmware_validator.h"
#include "flash.h"
#include "math.h"
#include "crc.h"
#include "image.h"

#if defined(UPDATER_BACKEND_CH376_UART)
    #include "storage_ch376_uart.h"
#elif defined(UPDATER_BACKEND_USB_OTG)
    #include "storage_usb_otg.h"
#endif

/* =========================================================================
 * Hata kodları
 * ========================================================================= */
typedef enum {
    UPDATER_ERR_OK              = 0U,
    UPDATER_ERR_INVALID_PARAM   = 1U
} Updater_Err_e;

typedef struct {
    // TODO: UpdaterNextion_Config_t nextion;
    FirmwareValidator_Config_t validator;

#if defined(UPDATER_BACKEND_CH376_UART)
    StorageCh376Uart_Config_t storage;
#elif defined(UPDATER_BACKEND_USB_OTG)
    StorageUsbOtg_Config_t storage;
#endif
} Updater_Config_t;

/* =========================================================================
 * Genel API
 * ========================================================================= */
/**
  * @brief Alt yapılandırmaları tüm modüllere dağıtır.
  * @param pCfg Tam olarak doldurulmuş bir Updater_Config_t yapısına işaretçi.
  * @retval NULL girişinde UPDATER_ERR_OK veya UPDATER_ERR_INVALID_PARAM.
  */
Updater_Err_e updater_init(const Updater_Config_t *pCfg);

/**
  * @brief Firmware güncelleme durum makinesini çalıştırır
  * @details Başarılı durumda hiçbir zaman geri dönmez.
  *         Kurtarılamayan herhangi bir hata durumunda hata kodu döndürür.
  * @retval Updater_Err_e (pratikte hiçbir zaman UPDATER_ERR_OK değildir).
  */
Updater_Err_e updater_run(void);

#ifdef __cplusplus
}
#endif

#endif /* UPDATER_H */