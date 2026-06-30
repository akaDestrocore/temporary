/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   storage_usb_otg.c
 * @brief  StorageApi için STM32F4 USB OTG FS host backend'i.
 *
 * @author  destrocore
 * @date    2026
 */

#include "storage_usb_otg.h"

#ifdef UPDATER_BACKEND_USB_OTG

#include "usbh_core.h"
#include "usbh_msc.h"
#include "usb_host.h"
#include "fatfs.h"
#include "ff.h"

/* ============================================================
 * Özel: modül durum sabitleri
 * ============================================================*/

// pBackend->init() çağrılmadan önce updater.c tarafından ayarlanır
static const StorageUsbOtg_Config_t *g_pCfg = NULL;

// usb_host.c dosyasına ait harici semboller
extern USBH_HandleTypeDef hUsbHostFS;
extern volatile ApplicationTypeDef Appli_state;

// Modüle özel durumlar
static FATFS gFatFs;
static FIL gFile;
static volatile bool gFileIsOpen = false;
static volatile bool gDeviceMounted = false;

/* ============================================================
 * Özel fonksiyon prototipleri
 * ============================================================*/
static void usbOtg_buildPath(char *pOut, size_t outSize, const char *pPath);
static StorageApi_Status_e usb_otg_init(void);
static StorageApi_Status_e usb_otg_isReady(void);
static StorageApi_Status_e usb_otg_fileOpen(const char *pPath);
static StorageApi_Status_e usb_otg_fileRead(uint8_t  *pBuff, uint32_t  buffSize, uint32_t *pBytesRead);
static StorageApi_Status_e usb_otg_fileClose(void);
static void usb_otg_deinit(void);


/* ============================================================
 * Backend tanımlayıcısı
 * ============================================================*/
static const StorageApi_Backend_t gUsbOtgBackend = {
    .init = usb_otg_init,
    .isReady = usb_otg_isReady,
    .fileOpen = usb_otg_fileOpen,
    .fileRead = usb_otg_fileRead,
    .fileClose = usb_otg_fileClose,
    .deinit = usb_otg_deinit,
};

/**
  * @brief USB OTG FS backend tanımlayıcısını döndürür
  * @retval Statik olarak ayrılmış StorageApi_Backend_t işaretçisi
  */
const StorageApi_Backend_t *storageApi_getBackend(void) {
    return &gUsbOtgBackend;
}

/**
  * @brief USB OTG FS backend'i için yapılandırmayı saklar
  * @param pCfg Doldurulmuş bir StorageUsbOtg_Config_t yapısına işaretçi
  * @retval None
  */
void storageUsbOtg_setConfig(const StorageUsbOtg_Config_t *pCfg) {
    g_pCfg = pCfg;
}

/**
  * @brief Saklanan yapılandırma işaretçisini döndürür
  * @retval StorageUsbOtg_Config_t yapısına işaretçi
  */
const StorageUsbOtg_Config_t *storageUsbOtg_getConfig(void) {
    return g_pCfg;
}

/* ============================================================
 * Kesmeler
 * ============================================================*/

 /**
  * @brief  OTG FS genel kesmesi
  * @retval None
  * @note İşlemi HAL HCD sürücüsüne devreder.
  */
void OTG_FS_IRQHandler(void) {
    HAL_HCD_IRQHandler((HCD_HandleTypeDef *)hUsbHostFS.pData);
}

/* ============================================================
 * Özel yardımcı fonksiyonlar
 * ============================================================*/

/**
  * @brief Sürücü prefiksini ekleyerek bir FatFS yolu oluşturur
  * @param pOut Hedef buffer (en az FF_MAX_LFN + 1 byte olmalı).
  * @param outSize pOut boyutu (byte cinsinden).
  * @param pPath Root dizine göre yol, örn. "/FIRMWARE.BIN"
  * @retval None
  */
static void usbOtg_buildPath(char *pOut, size_t outSize, const char *pPath) {
    size_t prefixLen = strlen(USBHPath);
    size_t pathLen = strlen(pPath);

    if ((prefixLen + pathLen + 1U) <= outSize) {
        (void)memcpy(pOut, USBHPath, prefixLen);
        (void)memcpy(pOut + prefixLen, pPath, pathLen + 1U); // +1 NUL
    } else {
        // Buffer beklenenden küçük - taşma yerine kırpma yapılır
        (void)strncpy(pOut, pPath, outSize - 1U);
        pOut[outSize - 1U] = '\0';
    }
}

/* ============================================================
 * StorageApi_Backend_t uygulaması
 * ============================================================*/

 /**
  * @brief USB OTG FS host ve FatFS middleware bileşenlerini başlatır
  * @retval STORAGE_STATUS_OK veya STORAGE_STATUS_NOT_READY
  * @note opt=1 ile herhangi bir f_mount() çağrısından önce 
  *     MX_FATFS_Init() MUTLAKA çağrılmalıdır!
  *     Bu fonksiyon, FATFS_LinkDriver() aracılığıyla USBH_Driver'ı
  *     FatFS'e kaydeder ve disk.drv[0] alanını doldurur.
  *     Bu çağrı yapılmazsa sürücü işaretçisi NULL kalır ve ilk
  *     f_mount çağrısında IBUSERR HardFault oluşur!
  */
static StorageApi_Status_e usb_otg_init(void) {
    gDeviceMounted = false;
    gFileIsOpen = false;

    // rcc_init() PLL'e (96 MHz) geçtikten sonra SysTick'i yeniden ayarlama
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000U);

    // USBH_Driver'ı FatFS'e kaydetme - f_mount'tan önce çağrılmalı
    MX_FATFS_Init();

    // USB host stack'ini başlatma, MSC sınıfını kaydetme ve host`unu çalıştırma
    MX_USB_HOST_Init();

    // HAL_HCD_MspInit ne ayarladıysa geçersiz kıl - OTG SysTick'in altında olmalı
    if (NULL != g_pCfg) {
        NVIC_SetPriority(OTG_FS_IRQn, g_pCfg->otgNvicPriority);
    }

    return STORAGE_STATUS_OK;
}

/**
  * @brief USB host durum makinesini yoklar ve hazır olduğunda FAT volume'ünü mount eder.
  * @retval FAT volume mount edilmiş ve hazırsa STORAGE_STATUS_OK.
  * @note `usb_host.c` içindeki USBH_UserProcess tarafından güncellenen
  *     Appli_state değişkenini yoklar.
  *     APPLICATION_READY görüldüğünde FAT volume mount edilir.
  *     APPLICATION_DISCONNECT durumunda unmount yapılır; böylece sonraki
  *     yeniden bağlantıda tekrar mount edilir.
  */
static StorageApi_Status_e usb_otg_isReady(void) {
    // "0:\0"
    char mountPath[8U];

    // İçinde USBH_Process çalıştırılır
    MX_USB_HOST_Process();

    if ((!gDeviceMounted) && (APPLICATION_READY == Appli_state)) {
        (void)strncpy(mountPath, USBHPath, sizeof(mountPath) - 1U);
        mountPath[sizeof(mountPath) - 1U] = '\0';

        if (FR_OK == f_mount(&gFatFs, mountPath, 1U)) {
            gDeviceMounted = true;
        }
        // f_mount başarısız olursa gDeviceMounted false kalır
        // sonraki çağrıda tekrar denenir
    }

    if (APPLICATION_DISCONNECT == Appli_state) {
        if (true == gDeviceMounted) {
            (void)f_mount(NULL, USBHPath, 0U);
            gDeviceMounted = false;
        }
    }

    return gDeviceMounted ? STORAGE_STATUS_OK : STORAGE_STATUS_NOT_READY;
}

/**
  * @brief Mount edilmiş FAT volume üzerinde mutlak yolla dosya açar.
  * @param pPath Root dizine göre yol, örn. "/FIRMWARE.BIN"
  * @retval STORAGE_STATUS_OK veya hata kodu.
  */
static StorageApi_Status_e usb_otg_fileOpen(const char *pPath) {
    char fullPath[FF_MAX_LFN + 1U];

    if (NULL == pPath) {
        return STORAGE_STATUS_INVALID_PARAM;
    }

    if (!gDeviceMounted) {
        return STORAGE_STATUS_NOT_READY;
    }

    usbOtg_buildPath(fullPath, sizeof(fullPath), pPath);

    if (FR_OK != f_open(&gFile, fullPath, FA_READ)) {
        return STORAGE_STATUS_FILE_NOT_FOUND;
    }
    gFileIsOpen = true;
    return STORAGE_STATUS_OK;
}

/**
  * @brief Açık dosyadan buffSize byte'a kadar veri okur.
  * @param pBuff Hedef buffer
  * @param buffSize Okunacak maksimum byte sayısı
  * @param pBytesRead pBuff içine yazılan gerçek byte sayısı; EOF durumunda 0
  * @retval STORAGE_STATUS_OK veya STORAGE_STATUS_READ_ERROR.
  */
static StorageApi_Status_e usb_otg_fileRead(uint8_t *pBuff, uint32_t buffSize, uint32_t *pBytesRead) {
    
    UINT bytesRead = 0U;

    if ((NULL == pBuff) || (NULL == pBytesRead) || (0U == buffSize)) {
        return STORAGE_STATUS_INVALID_PARAM;
    }

    if (!gFileIsOpen) {
        return STORAGE_STATUS_NOT_READY;
    }

    if (FR_OK != f_read(&gFile, pBuff, (UINT)buffSize, &bytesRead)) {
        return STORAGE_STATUS_READ_ERROR;
    }

    *pBytesRead = (uint32_t)bytesRead;
    return STORAGE_STATUS_OK;
}

/**
  * @brief  Şu anda açık olan dosyayı kapatır
  * @retval STORAGE_STATUS_OK her zaman döndürülür
  */
static StorageApi_Status_e usb_otg_fileClose(void) {
    if (true == gFileIsOpen) {
        (void)f_close(&gFile);
        gFileIsOpen = false;
    }

    return STORAGE_STATUS_OK;
}

/**
  * @brief  USB host'u durdurur, FatFS'i unmount eder ve OTG FS çevre birimini kapatır.
  * @retval None
  * @note g_pCfg NULL ise hiçbir işlem yapılmaz.
  */
static void usb_otg_deinit(void) {
    
    if (true == gDeviceMounted) {
        (void)f_mount(NULL, USBHPath, 0U);
        gDeviceMounted = false;
    }

    (void)USBH_Stop(&hUsbHostFS);
    (void)USBH_DeInit(&hUsbHostFS);

    NVIC_DisableIRQ(OTG_FS_IRQn);

    // VBUS enable pinini HIGH yapma -> aktif-LOW anahtar kapatma
    if ((NULL != g_pCfg) && (NULL != g_pCfg->pVbusEnPort)) {
        g_pCfg->pVbusEnPort->BSRR = g_pCfg->vbusEnPinBsrr;
    }

    RCC->AHB2ENR &= ~RCC_AHB2ENR_OTGFSEN;
}

#endif /* UPDATER_BACKEND_USB_OTG */