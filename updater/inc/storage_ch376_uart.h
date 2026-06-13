/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Elektrokoter Ünitesi                       ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   storage_ch376_uart.h
 * @brief  UART seri mod üzerinden CH376 USB host backend'i.
 *
 * @author  destrocore
 * @date    2026
 * 
 * @details
 *  Tüm pin ve çevre birimi yapılandırmaları, doğrudan `updater_config.h` 
 *  dosyasından okunmak yerine StorageCh376Uart_Config_t yapısı üzerinden 
 *  sağlanır.
 *  storageApi_getBackend() çağrılmadan önce storageCh376Uart_setConfig() 
 *  fonksiyonu bir kez çağrılmalıdır!
 *
 *  İşlem tamamlanma tespiti yalnızca polling yöntemiyle gerçekleştirilir.
 *  UART modunda INT pini bilerek kullanılmaz.
 */
#ifndef STORAGE_CH376_UART_H
#define STORAGE_CH376_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include "storage_api.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef UPDATER_BACKEND_CH376_UART

/* ============================================================
 * CH376 komut sabitleri
 * ============================================================*/
#define CH376U_CMD_SET_BAUDRATE     0x02U
#define CH376U_CMD_RESET_ALL        0x05U
#define CH376U_CMD_CHECK_EXIST      0x06U
#define CH376U_CMD_GET_FILE_SIZE    0x0CU
#define CH376U_CMD_SET_USB_MODE     0x15U
#define CH376U_CMD_GET_STATUS       0x22U
#define CH376U_CMD_RD_USB_DATA0     0x27U
#define CH376U_CMD_SET_FILE_NAME    0x2FU
#define CH376U_CMD_DISK_CONNECT     0x30U
#define CH376U_CMD_DISK_MOUNT       0x31U
#define CH376U_CMD_FILE_OPEN        0x32U
#define CH376U_CMD_FILE_CLOSE       0x36U
#define CH376U_CMD_BYTE_READ        0x3AU
#define CH376U_CMD_BYTE_RD_GO       0x3BU

/* ============================================================
 * CH376 kesme/status dönüş kodları
 * ============================================================*/
#define CH376U_INT_SUCCESS          0x14U
#define CH376U_INT_CONNECT          0x15U
#define CH376U_INT_DISCONNECT       0x16U
#define CH376U_INT_BUF_OVER         0x17U
#define CH376U_INT_DISK_READ        0x1DU
#define CH376U_RET_SUCCESS          0x51U

/* ============================================================
 * UART senkronizasyon baytları
 * ============================================================*/
#define CH376U_SYNC_BYTE0           0x57U
#define CH376U_SYNC_BYTE1           0xABU

/* ============================================================
 * USB host modu baytları
 * ============================================================*/
#define CH376U_USB_MODE_HOST_RESET  0x07U   // veri yolu sıfırlaması uygulanmış
#define CH376U_USB_MODE_HOST_SOF    0x06U   // veri yolu sıfırlaması kaldırılmış

/* ============================================================
 * Check-exist yankı
 * ============================================================*/
#define CH376U_CHECK_EXIST_SEND     0x65U
#define CH376U_CHECK_EXIST_EXPECT   0x9AU

/* ============================================================
 * BYTE_READ başına en yüksek bayt sayısı
 * ============================================================*/
#define CH376U_MAX_BYTES_PER_READ   64U

/* ============================================================
 * CH376 Timeout kodu
 * ============================================================*/
#define CH376U_STATUS_TIMEOUT       0xFFU

/**
 * @brief CH376 UART depolama backend için yapılandırma yapısı.
 *
 * @details
 *  Çağıran taraf bu yapıyı ilgili sabitlerle doldurur ve
 *  storageCh376Uart_setConfig() fonksiyonuna iletir.
 */
typedef struct {
    USART_TypeDef *pUsart;
    GPIO_TypeDef *pGpio_port;
    uint32_t tx_pin_num;
    uint32_t rx_pin_num;
    uint32_t gpio_af;
    uint32_t gpio_ahb1_bits;
    uint32_t apbx_bit;
    uint8_t is_apb2;
    uint32_t apb_freq_hz;
    uint32_t init_baud;
    uint32_t target_baud;
    uint8_t baud_rate_coef;
    uint8_t baud_rate_const;
    uint32_t read_timeout_ms;
} StorageCh376Uart_Config_t;


/* ============================================================
 * Backend APIleri
 * ============================================================*/
/**
  * @brief Backend için donanım yapılandırmasını saklar.
  * @param pCfg Doldurulmuş StorageCh376Uart_Config_t yapısına işaretçi.
  * @retval None
  * @details Backend fonksiyonları çağrılmadan önce çağrılmalıdır.
  */
void storageCh376Uart_setConfig(const StorageCh376Uart_Config_t *pCfg);

/**
  * @brief CH376-UART backend tanımlayıcısını döndürür.
  * @retval StorageApi_Backend_t nesnesine işaretçi.
  */
const StorageApi_Backend_t *storageApi_getBackend(void);

#endif // UPDATER_BACKEND_CH376_UART

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_CH376_UART_H */