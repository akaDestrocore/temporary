/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   storage_ch376_uart.c
 * @brief  StorageApi için CH376 USB host backend'i.
 *
 * @author  destrocore
 * @date    2026
 */
#include "storage_ch376_uart.h"

#ifdef UPDATER_BACKEND_CH376_UART

// storageCh376Uart_setConfig() tarafından sağlanır
static const StorageCh376Uart_Config_t *gpCh376uCfg = NULL;

/* ============================================================
 * Özel: modül durum sabitleri
 * ============================================================*/
#define CH376U_DISK_CONNECT_RETRY_MS    50U
#define CH376U_MAX_FILENAME_LEN         13U

static uint32_t gFileBytesRemaining = 0U;
static uint32_t gFileSize = 0U;

/* ============================================================
 * Özel fonksiyon prototipleri
 * ============================================================*/
static StorageApi_Status_e ch376u_init(void);
static void ch376u_uartInit(uint32_t baud);
static void ch376u_txByte(uint8_t data);
static int32_t ch376u_rxByte(uint8_t *pByte);
static int32_t ch376u_rxByteMs(uint8_t *pByte, uint32_t timeoutMs);
static void ch376u_sendCmd(uint8_t cmd);
static int32_t ch376u_setBaudrate(void);
static void ch376u_recoverBaud(void);
static StorageApi_Status_e ch376u_isReady(void);
static StorageApi_Status_e ch376u_fileOpen(const char *pPath);
static StorageApi_Status_e ch376u_fileRead(uint8_t *pBuff, uint32_t buffSize, uint32_t *pBytesRead);
static StorageApi_Status_e ch376u_fileClose(void);
static void ch376u_deinit(void);


/* ============================================================
 * Backend tanımlayıcısı
 * ============================================================*/
static const StorageApi_Backend_t gCh376uBackend = {
    
    .init = ch376u_init,
    .isReady = ch376u_isReady,
    .fileOpen = ch376u_fileOpen,
    .fileRead = ch376u_fileRead,
    .fileClose = ch376u_fileClose,
    .deinit = ch376u_deinit,
};

/**
  * @brief Backend için donanım yapılandırmasını saklar.
  * @param pCfg Doldurulmuş StorageCh376Uart_Config_t yapısına işaretçi.
  * @retval None
  * @details Backend fonksiyonları çağrılmadan önce çağrılmalıdır.
  */
void storageCh376Uart_setConfig(const StorageCh376Uart_Config_t *pCfg) {
    gpCh376uCfg = pCfg;
}

/**
  * @brief CH376-UART backend tanımlayıcısını döndürür.
  * @retval StorageApi_Backend_t nesnesine işaretçi.
  */
const StorageApi_Backend_t *storageApi_getBackend(void) {
    return &gCh376uBackend;
}

/* ============================================================
 * StorageApi_Backend_t uygulaması
 * ============================================================*/
static StorageApi_Status_e ch376u_init(void) {
    
    gFileBytesRemaining = 0;
    gFileSize = 0;

    ch376u_recoverBaud();
    ch376u_uartInit(gpCh376uCfg->init_baud);

    HAL_Delay(200U);
    (void)gpCh376uCfg->pUsart->SR;
    (void)gpCh376uCfg->pUsart->DR;

    // Echo test
    ch376u_sendCmd(CH376U_CMD_CHECK_EXIST);
    ch376u_txByte(CH376U_CHECK_EXIST_SEND);

    uint8_t echo = 0U;
    if ((0 != ch376u_rxByte(&echo)) || (CH376U_CHECK_EXIST_EXPECT != echo)) {
        return STORAGE_STATUS_NOT_READY;
    }

    if (gpCh376uCfg->init_baud != gpCh376uCfg->target_baud) {
        if (0 != ch376u_setBaudrate()) {
            return STORAGE_STATUS_NOT_READY;
        }
    }

    // USB bus resetleme
    ch376u_sendCmd(CH376U_CMD_SET_USB_MODE);
    ch376u_txByte(CH376U_USB_MODE_HOST_RESET);

    uint8_t modeResp = 0U;
    if ((0 != ch376u_rxByte(&modeResp)) || (CH376U_RET_SUCCESS != modeResp)) {
        return STORAGE_STATUS_NOT_READY;
    }

    HAL_Delay(50);

    // SOF başlatma
    ch376u_sendCmd(CH376U_CMD_SET_USB_MODE);
    ch376u_txByte(CH376U_USB_MODE_HOST_SOF);

    modeResp = 0U;
    if ((0 != ch376u_rxByte(&modeResp)) || (CH376U_RET_SUCCESS != modeResp)) {
        return STORAGE_STATUS_NOT_READY;
    }

    HAL_Delay(500);

    return STORAGE_STATUS_OK;
}

/**
  * @brief CH376 UART çevre birimin yapılandırması.
  * @param baud BRR hesaplaması için baud hızı.
  * @retval None
  */
static void ch376u_uartInit(uint32_t baud) {
    
    // GPIO saat açma
    RCC->AHB1ENR |= gpCh376uCfg->gpio_ahb1_bits;
    (void)RCC->AHB1ENR;

    // TX pin: AF, push-pull, medium speed, no pull
    {
        uint32_t pin = gpCh376uCfg->tx_pin_num;
        gpCh376uCfg->pGpio_port->MODER &= ~(3UL << (pin * 2U));
        gpCh376uCfg->pGpio_port->MODER |= (2UL << (pin * 2U));     // AF
        gpCh376uCfg->pGpio_port->OTYPER &= ~(1UL <<  pin);         // push-pull
        gpCh376uCfg->pGpio_port->OSPEEDR &= ~(3UL << (pin * 2U));
        gpCh376uCfg->pGpio_port->OSPEEDR |= (2UL << (pin * 2U));   // medium
        gpCh376uCfg->pGpio_port->PUPDR &= ~(3UL << (pin * 2U));     // no pull

        uint32_t afrIdx = (pin >= 8U) ? 1U : 0U;
        uint32_t afrShift = (pin % 8U) * 4U;
        gpCh376uCfg->pGpio_port->AFR[afrIdx] &= ~(0xFUL << afrShift);
        gpCh376uCfg->pGpio_port->AFR[afrIdx] |= (gpCh376uCfg->gpio_af << afrShift);
    }

    // RX pin: AF, pull-up
    {
        uint32_t pin = gpCh376uCfg->rx_pin_num;
        gpCh376uCfg->pGpio_port->MODER &= ~(3UL << (pin * 2U));
        gpCh376uCfg->pGpio_port->MODER |= (2UL << (pin * 2U));      // AF
        gpCh376uCfg->pGpio_port->OTYPER &= ~(1UL <<  pin);
        gpCh376uCfg->pGpio_port->OSPEEDR &= ~(3UL << (pin * 2U));
        gpCh376uCfg->pGpio_port->OSPEEDR |= (2UL << (pin * 2U));    // medium
        gpCh376uCfg->pGpio_port->PUPDR &= ~(3UL << (pin * 2U));
        gpCh376uCfg->pGpio_port->PUPDR |= (1UL << (pin * 2U));      // pull-up

        uint32_t afrIdx = (pin >= 8U) ? 1U : 0U;
        uint32_t afrShift = (pin % 8U) * 4U;
        gpCh376uCfg->pGpio_port->AFR[afrIdx] &= ~(0xFUL << afrShift);
        gpCh376uCfg->pGpio_port->AFR[afrIdx] |= (gpCh376uCfg->gpio_af << afrShift);
    }

    // USART çevre birimi saati (APB1 veya APB2)
    if (1U == gpCh376uCfg->is_apb2) {
        RCC->APB2ENR |= gpCh376uCfg->apbx_bit;
        (void)RCC->APB2ENR;
    } else {
        RCC->APB1ENR |= gpCh376uCfg->apbx_bit;
        (void)RCC->APB1ENR;
    }

    gpCh376uCfg->pUsart->CR1 = 0U;
    gpCh376uCfg->pUsart->CR2 = 0U;
    gpCh376uCfg->pUsart->CR3 = 0U;

    // BRR: tamsayı bölücü, 16 kat oversampling
    gpCh376uCfg->pUsart->BRR = gpCh376uCfg->apb_freq_hz / baud;

    // USART, TX ve RX etkinleştirme
    gpCh376uCfg->pUsart->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    // RE ilk setlendiğinde eski baytları sıfırlama
    (void)gpCh376uCfg->pUsart->SR;  
    (void)gpCh376uCfg->pUsart->DR;
}


/* ============================================================
 * Yardımcı fonksiyonlar
 * ============================================================*/

 /**
  * @brief Tek bayt gönderme.
  * @param data Gönderilecek bayt.
  * @retval None
  * @details TXE'de blocking.
  */
 static void ch376u_txByte(uint8_t data) {
    
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TXE)) {
        // TXE biti 0 olduğu sürece bekleme
    }

    gpCh376uCfg->pUsart->DR = (uint32_t)data;
}


/**
  * @brief Bir bayt alır ve kullanıcı tarafından verilen değişkenin adresine o veriyi yazar.
  * @param pByte Alınan bayt.
  * @retval Başarılı olursa 0, zaman aşımı durumunda -1.
  * @details Yalnızca 20 ms'den çok daha kısa sürede yanıt 
  *         verdiği bilinen komutlar için kullanılmalıdır. 
  *         Öncesinde SR'daki TC ve RXNE bitleri temizlenmeli.
  */
static int32_t ch376u_rxByte(uint8_t *pByte) {
    // 16 MHz'de 20 ms için 11428 iterasyon lazım
    uint32_t timeout = (16000000UL * 20UL/1000UL) / 28UL;
    
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
        
        if (0U == timeout) {
            return -1;
        }

        timeout--;
    }

    *pByte = (uint8_t)(gpCh376uCfg->pUsart->DR & 0xFFU);
    return 0;
 }

/**
  * @brief Milisaniye zaman aşımı desteğiyle bir bayt alır.
  * @param pByte Alınan bayt.
  * @param timeoutMs Milisaniye cinsinden maksimum bekleme süresi.
  * @retval Başarı durumunda 0, zaman aşımı durumunda -1.
  */
static int32_t ch376u_rxByteMs(uint8_t *pByte, uint32_t timeoutMs) {
    
    uint32_t startTick = HAL_GetTick();

    while ((HAL_GetTick() - startTick) < timeoutMs) {
        if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
            *pByte = (uint8_t)(gpCh376uCfg->pUsart->DR & 0xFFU);
            return 0;
        }
    }

    return -1;
}

/**
  * @brief İki baytlık UART senkronizasyon başlığını ve komut kodunu gönderir.
  * @param cmd CH376 komut işlem kodu.
  * @retval None
  */
static void ch376u_sendCmd(uint8_t cmd) {
    
    ch376u_txByte(CH376U_SYNC_BYTE0);
    ch376u_txByte(CH376U_SYNC_BYTE1);
    ch376u_txByte(cmd);
}

/**
  * @brief CH376S ve STM32 USART birimini hedef baud hızına geçirir.
  * @retval Başarı durumunda 0, zaman aşımı veya beklenmeyen yanıt durumunda -1.
  * @details CH376S yeni baud hızında ACK yanıtını hemen gönderir.
  *     Bu nedenle yanıt okunmadan önce STM32 BRR değeri güncellenir.
  */
static int32_t ch376u_setBaudrate(void) {
    
    uint8_t resp = 0U;

    ch376u_sendCmd(CH376U_CMD_SET_BAUDRATE);
    ch376u_txByte(gpCh376uCfg->baud_rate_coef);
    ch376u_txByte(gpCh376uCfg->baud_rate_const);

    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
        // Shift registerdaki son stop bit çıkmadan BRR değiştirilmemeli
    }

    // ACK yeni baud hızında gönderileceği için STM32 USART yeniden yapılandırılır
    gpCh376uCfg->pUsart->CR1 &= ~USART_CR1_UE;
    gpCh376uCfg->pUsart->BRR = gpCh376uCfg->apb_freq_hz / gpCh376uCfg->target_baud;
    gpCh376uCfg->pUsart->CR1 |= USART_CR1_UE;

    HAL_Delay(2U);

    if (0 != ch376u_rxByte(&resp)) {
        return -1;
    }

    if (CH376U_RET_SUCCESS != resp) {
        return -1;
    }

    return 0;
}

 /**
  * @brief CH376'nın önceki bir oturumdan target_baud değerini 
  * koruyup korumadığını kontrol eder ve koruyorsa açılışta 
  * kullanılan init_baud varsayılanına geri için RESET_ALL komutunu 
  * gönderir.
  * @retval None
  * @details Bu fonksiyon döndükten sonra ch376u_init(), 
  *     ch376u_uartInit(init_baud) fonksiyonunu çağırarak çevre 
  *     birimini doğru çalışma baud hızında yeniden başlatır.
  */
static void ch376u_recoverBaud(void) {
    
    uint8_t echo = 0U;
    
    if (gpCh376uCfg->init_baud == gpCh376uCfg->target_baud) {
        return;
    }

    ch376u_uartInit(gpCh376uCfg->target_baud);

    ch376u_sendCmd(CH376U_CMD_CHECK_EXIST);
    ch376u_txByte(CH376U_CHECK_EXIST_SEND);

    bool atTargetBaud = (0 == ch376u_rxByte(&echo)) && (CH376U_CHECK_EXIST_EXPECT == echo);

    if (true == atTargetBaud) {
        // init_baud değerini resetle
        ch376u_sendCmd(CH376U_CMD_RESET_ALL);
        
        while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
            // USART transfer tamamlanana kadar bekleme
        }

        HAL_Delay(200U);
    }
}

/**
  * @brief USB depolama aygıtının bağlanmış ve hazır olup olmadığını sorgular.
  * @retval STORAGE_STATUS_OK veya STORAGE_STATUS_NOT_READY.
  * @details UART modunda CH376S her komuttan sonra durum baytını doğrudan döndürür.
  *     CMD_GET_STATUS komutu KULLANILMAMALIDIR.
  *     CH376S’e aşırı komut gönderimini önlemek için NOT_READY döndürmeden önce 
  *     CH376U_DISK_CONNECT_RETRY_MS gecikmesi uygulanır.
  */
static StorageApi_Status_e ch376u_isReady(void) {
    
    uint8_t status = 0U;

    ch376u_sendCmd(CH376U_CMD_DISK_CONNECT);
    if ((0 != ch376u_rxByte(&status)) || (CH376U_INT_SUCCESS != status)) {    
        HAL_Delay(CH376U_DISK_CONNECT_RETRY_MS);
        return STORAGE_STATUS_NOT_READY;
    }

    ch376u_sendCmd(CH376U_CMD_DISK_MOUNT);
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
        // USART transfer tamamlanana kadar bekleme
    }

    if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
        (void)gpCh376uCfg->pUsart->DR;
    }

    if ((0 != ch376u_rxByteMs(&status, 2000U)) || (CH376U_INT_SUCCESS != status)) {
        return STORAGE_STATUS_NOT_READY;
    }

    return STORAGE_STATUS_OK;
}

/**
  * @brief Dosya adını ayarlar, CH376 üzerinde dosyayı açar ve boyutunu alır.
  * @param pPath NUL ile sonlandırılmış dosya yolu (ör. "/FIRMWARE.BIN").
  * @retval StorageApi_Status_OK veya StorageApi_Status_FILE_NOT_FOUND.
  */
static StorageApi_Status_e ch376u_fileOpen(const char *pPath) {
    
    if (NULL == pPath) {
        return STORAGE_STATUS_INVALID_PARAM;
    }

    gFileBytesRemaining = 0U;
    gFileSize = 0U;

    // SET_FILE_NAME
    ch376u_sendCmd(CH376U_CMD_SET_FILE_NAME);
    uint32_t i = 0;
    while (('\0' != pPath[i]) && (i < CH376U_MAX_FILENAME_LEN)) {
        ch376u_txByte((uint8_t)pPath[i]);
        i++;
    }

    // Sonuna NUL ekleme
    ch376u_txByte(0U);

    // FILE_OPEN
    uint8_t status = 0U;
    ch376u_sendCmd(CH376U_CMD_FILE_OPEN);
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
        // USART transfer tamamlanana kadar bekleme
    }

    if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
        (void)gpCh376uCfg->pUsart->DR;
    }

    if ((0U != ch376u_rxByteMs(&status, 2000U)) || (CH376U_INT_SUCCESS != status)) {
        return STORAGE_STATUS_FILE_NOT_FOUND;
    }

    // Dosya boyut verisi çıkartma
    ch376u_sendCmd(CH376U_CMD_GET_FILE_SIZE);
    ch376u_txByte(0x68U);
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
        // USART transfer tamamlanana kadar bekleme
    }

    if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
        (void)gpCh376uCfg->pUsart->DR;
    }

    uint8_t sizeBuff[4U] = {0U, 0U, 0U, 0U};
    for (uint8_t b = 0U; b < 4U; b++) {
        if (0 != ch376u_rxByte(&sizeBuff[b])) {
            return STORAGE_STATUS_FILE_NOT_FOUND;
        }
    }

    gFileSize = ((uint32_t)sizeBuff[3U] << 24U) 
                | (uint32_t)(sizeBuff[2U] << 16U)
                | (uint32_t)(sizeBuff[1U] << 8U)
                | (uint32_t)(sizeBuff[0U]);

    gFileBytesRemaining = gFileSize;
    return STORAGE_STATUS_OK;
}

/**
  * @brief Açık dosyadan BYTE_READ aracılığıyla veri okur.
  * @param pBuff Hedef buffer.
  * @param buffSize Okunacak bayt sayısı.
  * @param pBytesRead pBuff içerisine gerçekten yazılan bayt sayısı.
  *                     Dosya sonuna (EOF) ulaşıldığında 0 olur.
  * @retval STORAGE_STATUS_OK veya STORAGE_STATUS_READ_ERROR.
  * @details Her BYTE_READ oturumu, yeni bir BYTE_READ komutu gönderilmeden önce
  *         tamamen tüketilmelidir.
  */
static StorageApi_Status_e ch376u_fileRead(uint8_t *pBuff, uint32_t buffSize, uint32_t *pBytesRead) {
    
    if ((NULL == pBuff) || (NULL == pBytesRead) || (0U == buffSize)) {
        return STORAGE_STATUS_INVALID_PARAM;
    }

    *pBytesRead = 0U;
    uint32_t remaining = buffSize;
    uint8_t *pDest = pBuff;

    while (0U < remaining) {
        uint16_t toRequest = (uint16_t)(remaining < (uint32_t)CH376U_MAX_BYTES_PER_READ) \
                                    ? remaining : (uint32_t)CH376U_MAX_BYTES_PER_READ;

        ch376u_sendCmd(CH376U_CMD_BYTE_READ);
        ch376u_txByte((uint8_t)(toRequest & 0xFFU));
        ch376u_txByte((uint8_t)((toRequest >> 8U) & 0xFFU));
        while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
            // USART transfer tamamlanana kadar bekleme
        }

        if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
            (void)gpCh376uCfg->pUsart->DR;
        }

        uint8_t status = 0U;
        if (0 != ch376u_rxByteMs(&status, gpCh376uCfg->read_timeout_ms)) {
            return STORAGE_STATUS_READ_ERROR;
        }

        if (CH376U_INT_SUCCESS == status) {
            break;
            // En baştan EOF'tan başka herhangi bir bayt gelmediyse bu oturumun başlamadığı anlamına gelir
        }

        if (CH376U_INT_DISK_READ != status) {
            return STORAGE_STATUS_READ_ERROR;
        }

        bool sessionDone = false;
        while (true != sessionDone) {
            ch376u_sendCmd(CH376U_CMD_RD_USB_DATA0);
            while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
                // USART transfer tamamlanana kadar bekleme
            }

            if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
                (void)gpCh376uCfg->pUsart->DR;
            }

            uint8_t count = 0U;
            if (0U != ch376u_rxByte(&count)) {
                return STORAGE_STATUS_READ_ERROR;
            }

            for (uint8_t b = 0U; b < count; b++) {
                uint8_t rxByte = 0U;
                if (0 != ch376u_rxByte(&rxByte)) {
                    return STORAGE_STATUS_READ_ERROR;
                }
                *pDest = rxByte;
                pDest++;
            }

            *pBytesRead += (uint32_t)count;
            remaining -= (remaining >= (uint32_t)count) ? (uint32_t)count : remaining;

            if (0U == count) {
                sessionDone = true;
                break;
            }

            ch376u_sendCmd(CH376U_CMD_BYTE_RD_GO);

            while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
                // USART transfer tamamlanana kadar bekleme
            }

            if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
                (void)gpCh376uCfg->pUsart->DR;
            }

            uint8_t rdGoStatus = 0U;
            if(0U != ch376u_rxByteMs(&rdGoStatus, gpCh376uCfg->read_timeout_ms)) {
                return STORAGE_STATUS_READ_ERROR;
            }

            if (CH376U_INT_SUCCESS == rdGoStatus) {
                // Bütün toRequest baytları alındı
                sessionDone = true;
            } else if (CH376U_INT_DISK_READ == rdGoStatus) {
                // Disk okuma işlemi devam ediyor
                // RD_USB_DATA0
            } else {
                return STORAGE_STATUS_READ_ERROR;
            }
        }
    }

    if (gFileBytesRemaining >= *pBytesRead) {
        gFileBytesRemaining -= *pBytesRead;
    } else {
        gFileBytesRemaining = 0U;
    }

    return STORAGE_STATUS_OK;
}

/**
  * @brief Şu anda açık olan dosyayı kapatır.
  * @retval STORAGE_STATUS_OK.
  */
static StorageApi_Status_e ch376u_fileClose(void) {
    
    ch376u_sendCmd(CH376U_CMD_FILE_CLOSE);
    ch376u_txByte(0x00U);
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
        // USART transfer tamamlanana kadar bekleme
    }

    if (0U != (gpCh376uCfg->pUsart->SR & USART_SR_RXNE)) {
        (void)gpCh376uCfg->pUsart->DR;
    }

    uint8_t dummy = 0U;
    (void)ch376u_rxByteMs(&dummy, 500U);

    gFileBytesRemaining = 0U;
    gFileSize = 0U;

    return STORAGE_STATUS_OK;
}

/**
  * @brief Bus saatini kapatarak UART çevre birimini devre dışı bırakır.
  * @retval None
  */
static void ch376u_deinit(void) {

    ch376u_sendCmd(CH376U_CMD_RESET_ALL);
    while (0U == (gpCh376uCfg->pUsart->SR & USART_SR_TC)) {
        // USART transfer tamamlanana kadar bekleme
    }

    HAL_Delay(200U);

    gpCh376uCfg->pUsart->CR1 = 0U;
    
    if (1U == gpCh376uCfg->is_apb2) {
        RCC->APB2ENR &= ~gpCh376uCfg->apbx_bit;
    } else {
        RCC->APB1ENR &= ~gpCh376uCfg->apbx_bit;
    }
}

#endif /* UPDATER_BACKEND_CH376_UART */