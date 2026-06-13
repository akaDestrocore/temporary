/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   storage_usb_otg.h
 * @brief  StorageApi için STM32F4 USB OTG FS host backend'i.
 *
 * @author  destrocore
 * @date    2026
 * 
 * @details
 *  Bu header dosyası middleware bağımsızdır. Sadece StorageApi ve pin 
 *  bağlantıları tanımlayan konfigürasyon yapısını içerir. USB middleware tiplerine ihtiyaç
 *  duyan derleme birimleri (HCD handle'ları, USBH callback'leri,
 *  FatFS) bu header yerine ilgili USB/FatFS header'larını doğrudan
 *  dahil etmelidir ve bu dosyanın dolaylı include zincirine güvenmemelidir.
 *
 *  DONANIM:
 *    PA11 — USB_OTG_FS_DM  (AF10)
 *    PA12 — USB_OTG_FS_DP  (AF10)
 *    PA9  — USB_OTG_FS_VBUS algılama (giriş)
 *    pVbusEnPort/vbusEnPinBsrr — harici VBUS anahtarı
 */

#ifndef STORAGE_USB_OTG_H
#define STORAGE_USB_OTG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f407xx.h"
#include "storage_api.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef UPDATER_BACKEND_USB_OTG

/**
 * @brief USB OTG FS depolama backend'i için donanım yapılandırması.
 *
 * @details
 *  `updater.c` tarafından `updater_config.h` sabitlerinden doldurulur ve
 *  `storageUsbOtg_setConfig()` fonksiyonuna aktarılır.
 *
 *  pVbusEnPort / vbusEnPinBsrr aktif-LOW harici VBUS güç anahtarını
 *  tanımlar:
 *    - HIGH yapılırsa (BSRR = vbusEnPinBsrr) -> VBUS kapalı
 *    - LOW yapılırsa  (BSRR = vbusEnPinBsrr << 16U) -> VBUS açık
 */
typedef struct {
    GPIO_TypeDef *pVbusEnPort;  // OTG_FS_PowerSwitchOn port (GPIOC)
    uint32_t vbusEnPinBsrr;     // PC0
    uint32_t otgNvicPriority;
} StorageUsbOtg_Config_t;

/**
  * @brief USB OTG FS backend'i için kart yapılandırmasını saklar.
  * @param pCfg Tam olarak doldurulmuş bir StorageUsbOtg_Config_t yapısına işaretçi.
  * @retval None
  * @note storageApi_getBackend()->init() çağrılmadan önce çağrılmalıdır!
  */
void storageUsbOtg_setConfig(const StorageUsbOtg_Config_t *pCfg);

/**
  * @brief Saklanan kart yapılandırması işaretçisini döndürür.
  * @retval StorageUsbOtg_Config_t yapısına işaretçi
  */
const StorageUsbOtg_Config_t *storageUsbOtg_getConfig(void);

/**
  * @brief USB OTG FS backend tanımlayıcısını döndürür
  * @retval Statik olarak ayrılmış bir StorageApi_Backend_t yapısına işaretçi.
  */
const StorageApi_Backend_t *storageApi_getBackend(void);

#endif /* UPDATER_BACKEND_USB_OTG */

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_USB_OTG_H */