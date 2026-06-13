/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file            updater.c
 * @brief           Firmware güncelleyicisinin ana giriş noktası ve güncelleme 
 *                  durum makinesi.
 *
 * @author          destrocore
 * @date            2026
 * 
 * @details
 *  Akış:
 *    1. rcc_init() - saat yapılandırması (CH376 backend'leri için HSI; USB OTG için PLL)
 *    2. HAL_Init() - 1 ms SysTick, flash.c için gereklidir (HAL_GetTick)
 *    3. updaterNextion_init()  - progress bar için bloklayıcı TX UART
 *    4. Depolama backend başlatması + USB cihazını bekleme
 *    5. UPDATER_FIRMWARE_FILENAME dosyasını açma
 *    6. firmwareValidator_run() - magic / tip / boyut / CRC doğrulaması
 *    7. Dosyayı yeniden açma, başlığı atlama, başlık + veriyi APP flash alanına yazma
 *    8. crc_verifyFirmware() - flash içindeki son CRC doğrulama kapısı
 *    9. NVIC_SystemReset() - bootloader yeni imajı başlatır
 *
 */

#include "updater.h"

/* ============================================================
 * Özel sabitler
 * ============================================================*/
static Updater_Config_t gCfg;
static uint8_t gHeaderBuff[IMAGE_HDR_SIZE];
static uint8_t gChunkBuff[UPDATER_CHUNK_SIZE];

/* ============================================================
 * Özel fonksiyon prototipleri
 * ============================================================*/
Updater_Err_e updater_init(const Updater_Config_t *pCfg);
Updater_Err_e updater_run(void);
static void rcc_init(void);
static void handleError(const StorageApi_Backend_t *pBackend, const char *pMsg);
void SysTick_Handler(void);
void Error_Handler(void);
void HardFault_Handler(void);

/* ============================================================
 * Genel API
 * ============================================================*/

/**
  * @brief  Giriş noktası.
  * @retval int Normal çalışma sırasında asla geri dönmez.
  */
int main(void) {
    
    __enable_irq();

    rcc_init();

    if (HAL_OK != HAL_Init()) {
        Error_Handler();
    }

    Updater_Config_t cfg;
    (void)memset(&cfg, 0, sizeof(cfg));

    // -- Firmware validator ---------------------------------------
    cfg.validator.app_flash_base = (uint32_t)APP_FLASH_BASE;
    cfg.validator.app_flash_max_size = (uint32_t)APP_FLASH_MAX_SIZE;

    // -- Backend'e göre depolama ----------------------------------
#if defined(UPDATER_BACKEND_CH376_UART)
    cfg.storage.pUsart = CH376_UART_INSTANCE;
    cfg.storage.apbx_bit = CH376_UART_APBx_BIT;
    cfg.storage.is_apb2 = (uint8_t)CH376_UART_IS_APB2;
    cfg.storage.pGpio_port = CH376_UART_GPIO_PORT;
    cfg.storage.gpio_ahb1_bits = CH376_UART_GPIO_AHB1_BITS;
    cfg.storage.tx_pin_num = CH376_UART_TX_PIN_NUM;
    cfg.storage.rx_pin_num = CH376_UART_RX_PIN_NUM;
    cfg.storage.gpio_af = CH376_UART_GPIO_AF;
    cfg.storage.apb_freq_hz = 16000000U;   // HSI 16 MHz
    cfg.storage.init_baud = CH376_UART_BAUD;
    cfg.storage.target_baud = CH376_UART_TARGET_BAUD;
    cfg.storage.baud_rate_coef = (uint8_t)CH376_UART_TARGET_BAUD_COEF;
    cfg.storage.baud_rate_const = (uint8_t)CH376_UART_TARGET_BAUD_CONST;
    cfg.storage.read_timeout_ms = UPDATER_READ_TIMEOUT_MS;
#elif defined(UPDATER_BACKEND_USB_OTG)
    cfg.storage.pVbusEnPort = USB_VBUS_EN_PORT;
    cfg.storage.vbusEnPinBsrr = (uint32_t)USB_VBUS_EN_PIN_BSRR;
    cfg.storage.otgNvicPriority = 6U;
#endif

    // -- Config uygulama ve başlatma ------------------------------
    Updater_Err_e err = updater_init(&cfg);
    if (UPDATER_ERR_OK != err) {
        Error_Handler();
    }

    err = updater_run();

    return (int)err;
}

/** @brief RCC çevre birimlerini başlatır
  * @retval None  
  */
static void rcc_init(void) {

    RCC->CR |= RCC_CR_HSION;
    while (0U == (RCC->CR & RCC_CR_HSIRDY)) {
        // HSI bekleme
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
        // HSI saat seçimi bekleme
    }

    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    (void)RCC->CFGR;

#if defined(UPDATER_BACKEND_USB_OTG)
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | ((uint32_t)UPDATER_USB_FLASH_LATENCY);
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != (uint32_t)UPDATER_USB_FLASH_LATENCY) {
        // FLASH bekleme
    }

    RCC->CR &= ~RCC_CR_PLLON;
    while (0U != (RCC->CR & RCC_CR_PLLRDY)) {
        // PLL bekleme
    }

    RCC->PLLCFGR = ((uint32_t)UPDATER_USB_PLL_M) |
        ((uint32_t)UPDATER_USB_PLL_N << RCC_PLLCFGR_PLLN_Pos) |
        ((uint32_t)UPDATER_USB_PLL_P << RCC_PLLCFGR_PLLP_Pos) |
        ((uint32_t)UPDATER_USB_PLL_Q << RCC_PLLCFGR_PLLQ_Pos) |
        RCC_PLLCFGR_PLLSRC_HSI;

    RCC->CR |= RCC_CR_PLLON;
    while (0U == (RCC->CR & RCC_CR_PLLRDY)) {}

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    SystemCoreClock = 96000000U;

#else
    RCC->AHB2ENR &= ~RCC_AHB2ENR_OTGFSEN;
    (void)RCC->AHB2ENR;

    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_0WS;
    SystemCoreClock = 16000000U;
#endif

    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;
}

 /**
  * @brief Çağıranın yapılandırmasını saklar ve tüm modülleri başlatır.
  * @param pCfg Doldurulmuş bir Updater_Config_t.
  * @retval UPDATER_ERR_OK veya UPDATER_ERR_INVALID_PARAM.
  */
Updater_Err_e updater_init(const Updater_Config_t *pCfg) {
    
    if (NULL == pCfg) {
        return UPDATER_ERR_INVALID_PARAM;
    }

    gCfg = *pCfg;

    //TODO: updaterNextion_init(&gCfg.nextion);
    firmwareValidator_setConfig(&gCfg.validator);

#if defined(UPDATER_BACKEND_CH376_UART)
    storageCh376Uart_setConfig(&gCfg.storage);
#elif defined(UPDATER_BACKEND_USB_OTG)
    storageUsbOtg_setConfig(&gCfg.storage);
#endif

    return UPDATER_ERR_OK;
}

/**
  * @brief Tam firmware güncelleme sırasını çalıştırır.
  * @retval Updater_Err_e (başarıda hiçbir zaman geri dönmez).
  */
Updater_Err_e updater_run(void) {
    
    // TODO: updaterNextion_sendProgress(0U);
    
    // --  Backend ilklendirme ------------------------------------
    const StorageApi_Backend_t *pBackend = storageApi_getBackend();
    StorageApi_Status_e storageStatus = pBackend->init();

    if (STORAGE_STATUS_OK != storageStatus) {
        handleError(pBackend, "İlklendirme başarısız.");
    }

    // --  USB aygıtı yoklama ------------------------------------
    uint32_t mountDeadline = HAL_GetTick() + UPDATER_MOUNT_TIMEOUT_MS;
    while(STORAGE_STATUS_OK != pBackend->isReady()) {
        if (HAL_GetTick() > mountDeadline) {
            handleError(pBackend, "USB bulunamadı.");
        }
    }

    // TODO: updaterNextion_sendProgress(5U);
    
    // --  Firmware dosyası açma ------------------------------------
    storageStatus = pBackend->fileOpen(UPDATER_FIRMWARE_FILENAME);
    if (STORAGE_STATUS_OK != storageStatus) {
        handleError(pBackend, "Dosya bulunamadı.");
    }

    //TODO: updaterNextion_sendProgress(10U);

    // --  Başlık, tür, boyut ve CRC doğrulaması ----------------------
    FirmwareValidator_Info_t info;
    memset(&info, 0, sizeof(FirmwareValidator_Info_t));

    FirmwareValidator_Result_e valResult = firmwareValidator_run(pBackend, &info);
    if (FIRMWARE_VALIDATOR_RESULT_OK != valResult) {
        (void)pBackend->fileClose();
        switch (valResult) {
            case FIRMWARE_VALIDATOR_RESULT_BAD_MAGIC:{
                handleError(pBackend, "Hatalı magic");
                break;
            }
            case FIRMWARE_VALIDATOR_RESULT_BAD_TYPE:{
                handleError(pBackend, "Hatalı imaj tipi");
                break;
            }
            case FIRMWARE_VALIDATOR_RESULT_BAD_SIZE:{
                handleError(pBackend, "Hatalı veri boyutu");
                break;
            }
            case FIRMWARE_VALIDATOR_RESULT_BAD_CRC:{
                handleError(pBackend, "CRC eşleşmedi");
                break;
            }
            case FIRMWARE_VALIDATOR_RESULT_READ_ERROR:{
                handleError(pBackend, "Veri okunamadı");
                break;
            }
            default:{
                handleError(pBackend, "Bilinmeyen hata");
                break;
            }
        }
    }

    // -- Yüklemeden önce dosya konumunu sıfırlamak için yeniden açma
    (void)pBackend->fileClose();
    storageStatus = pBackend->fileOpen(UPDATER_FIRMWARE_FILENAME);
    if (STORAGE_STATUS_OK != storageStatus) {
        handleError(pBackend, "Yeniden açma başarısız");
    }

    // -- İmaj başlığı okuma -------------------------------------
    uint32_t bytesRead = 0U;
    storageStatus = pBackend->fileRead(gHeaderBuff, (uint32_t)(IMAGE_HDR_SIZE), &bytesRead);

    if ((STORAGE_STATUS_OK != storageStatus) || (uint32_t)(IMAGE_HDR_SIZE) != bytesRead) {
        (void)pBackend->fileClose();
        handleError(pBackend, "İmaj başlığı okunamadı");
    }

    // -- Uygulama flash bölgesini silme ------------------------
    // TODO: updaterNextion_sendProgress(15U);

    flash_Status_e flashStatus = flash_erase(gCfg.validator.app_flash_base);
    if (FLASH_OK != flashStatus) {
        (void)pBackend->fileClose();
        handleError(pBackend, "Flash bölgesi silme başarısız");
    }

    // -- İmaj başlığını flash belleğe yazma ----------------------
    // TODO: updaterNextion_sendProgress(20U);

    flashStatus = flash_write(gCfg.validator.app_flash_base, gHeaderBuff, (size_t)(IMAGE_HDR_SIZE));
    if (FLASH_OK != flashStatus) {
        (void)pBackend->fileClose();
        handleError(pBackend, "Başlık yazma başarısız");
    }

    // -- Veri bölümünü akış halinde flaşlama ----------------------
    uint32_t totalWritten = 0U;
    uint32_t flashAddr = gCfg.validator.app_flash_base + (uint32_t)IMAGE_HDR_SIZE;

    while (totalWritten < info.data_size) {
        uint32_t toRead = info.data_size - totalWritten;
        if (toRead > (uint32_t)UPDATER_CHUNK_SIZE) {
            toRead = (uint32_t)UPDATER_CHUNK_SIZE;
        }

        bytesRead = 0U;
        storageStatus = pBackend->fileRead(gChunkBuff, toRead, &bytesRead);
        if ((STORAGE_STATUS_OK != storageStatus) || (0U == bytesRead)) {
            (void)pBackend->fileClose();
            handleError(pBackend, "Veri okuma başarısız");
        }

        // 4'e hizalama
        uint32_t writeLen = (bytesRead + 3U) & ~3U;
        if (writeLen > bytesRead) {
            (void)memset(&gChunkBuff[bytesRead], 0U, (size_t)(writeLen - bytesRead));
        }

        flashStatus = flash_write(flashAddr, gChunkBuff, (size_t)writeLen);
        if (FLASH_OK != flashStatus) {
            (void)pBackend->fileClose();
            handleError(pBackend, "Flas yazma başarısız");
        }

        flashAddr += writeLen;
        totalWritten += bytesRead;

        uint8_t percent = (uint8_t)(20 + ((totalWritten * 75) / info.data_size));
        // TODO: updaterNextion_sendProgress(percent);
    }

    (void)pBackend->fileClose();
    pBackend->deinit();

    // -- Flash bellekte bulunan imajı doğrulama -------------------
    // TODO: updaterNextion_sendProgress(96U);

    crc_Status_e crcStatus = crc_verifyFirmware(gCfg.validator.app_flash_base, (uint32_t)IMAGE_HDR_SIZE);
    if(CRC_OK != crcStatus) {
        crc_invalidateFirmware(gCfg.validator.app_flash_base);
        handleError(NULL, "CRC doğrulama başarısız");
    }

    // TODO: updaterNextion_sendProgress(100U);
    // TODO: add jump later
    HAL_Delay(2000U);
    NVIC_SystemReset();

    return UPDATER_ERR_OK;
}

/**
  * @brief Depolama katmanını sonlandırır (açıksa) ve sonsuz döngüye girer.
  * @param pBackend Aktif depolama backend'i; NULL olabilir.
  * @param pMsg Null-sonlandırılmış hata mesajı.
  * @retval None.
  */
static void handleError(const StorageApi_Backend_t *pBackend, const char *pMsg) {
    
    if (NULL != pBackend) {
        pBackend->deinit();
    }
    
    // TODO: updaterNextion_sendStatus(pMsg);
    while(1) {
        __NOP();
    }
}

/**
  * @brief  SysTick ISR — HAL milisaniye sayacını ilerletir.
  * @retval None
  */
void SysTick_Handler(void) {
    HAL_IncTick();
}

/* ============================================================
 * Hata ve arıza işleyicileri
 * ============================================================*/
/**
  * @brief Genel HAL uygulama hata işleyicisi.
  * @retval None
  */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        __NOP();
    }
}

/**
  * @brief HardFault işleyicisi — hata ayıklayıcı için fault register'larını yakalar.
  * @retval None
  */
void HardFault_Handler(void) {
    volatile uint32_t cfsr = SCB->CFSR;
    volatile uint32_t hfsr = SCB->HFSR;
    volatile uint32_t bfar = SCB->BFAR;
    volatile uint32_t mmar = SCB->MMFAR;
    (void)cfsr; 
    (void)hfsr; 
    (void)bfar; 
    (void)mmar;
    while (1) {
        __NOP();
    }
}