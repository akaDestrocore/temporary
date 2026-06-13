#ifndef UPDATER_CONFIG_H
#define UPDATER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if !defined(UPDATER_BACKEND_CH376) && \
    !defined(UPDATER_BACKEND_CH376_UART) && \
    !defined(UPDATER_BACKEND_USB_OTG)
    #define UPDATER_BACKEND_CH376_UART
#endif /* !defined(UPDATER_BACKEND_CH376) && ... */

#if (defined(UPDATER_BACKEND_CH376) && defined(UPDATER_BACKEND_CH376_UART)) || \
    (defined(UPDATER_BACKEND_CH376) && defined(UPDATER_BACKEND_USB_OTG)) || \
    (defined(UPDATER_BACKEND_CH376_UART) && defined(UPDATER_BACKEND_USB_OTG))
    #error "Define exactly one of UPDATER_BACKEND_CH376, UPDATER_BACKEND_CH376_UART, or UPDATER_BACKEND_USB_OTG"
#endif /* (defined(UPDATER_BACKEND_CH376) && defined(UPDATER_BACKEND_CH376_UART)) || ... */

/* =========================================================================
 * Zamanlama sabitleri (ms)
 * ========================================================================= */
#define UPDATER_MOUNT_TIMEOUT_MS    10000U
#define UPDATER_READ_TIMEOUT_MS     3000U

/* =========================================================================
 * Firmware dosya tanımlaması
 * ========================================================================= */
#define UPDATER_FIRMWARE_FILENAME "/FIRMWARE.BIN"

/* =========================================================================
 * Dosya I/O parça boyutu (4'e hizalı olmalıdır)
 * ========================================================================= */
#define UPDATER_CHUNK_SIZE 0x200U

/* =========================================================================
 * Uygulama FLASH bölgesi
 * CMakeLists.txt APP_BASE ve linker/app.ld MEMORY bloğuyla aynı olmalı
 * ========================================================================= */
#define APP_FLASH_BASE  0x08020000UL
#define APP_FLASH_END   0x08100000UL
#define APP_FLASH_MAX_SIZE  (APP_FLASH_END - APP_FLASH_BASE)

/* ================================================================
 * UPDATER_BACKEND_USB_OTG
 * ================================================================*/
#ifdef UPDATER_BACKEND_USB_OTG

#define USB_VBUS_EN_PORT            GPIOC
#define USB_VBUS_EN_PIN_BSRR        (1UL << 0U) // PC0

// PLL: HSI(16)/8 * 96 / 2 = 96 MHz SYSCLK;  HSI(16)/8 * 96 / 4 = 48 MHz USB
#define UPDATER_USB_PLL_M           8U
#define UPDATER_USB_PLL_N           96U
#define UPDATER_USB_PLL_P           0U      // 0x0 → PLLP divider = 2
#define UPDATER_USB_PLL_Q           4U
#define UPDATER_USB_FLASH_LATENCY   3U      // 96 MHz -> 3 WS

#endif /* UPDATER_BACKEND_USB_OTG */

/* ================================================================
 * UPDATER_BACKEND_CH376_UART
 * ================================================================*/
#ifdef UPDATER_BACKEND_CH376_UART

// USART çevre birimi
#define CH376_UART_INSTANCE             USART1
#define CH376_UART_IS_APB2              1U
#define CH376_UART_APBx_BIT             RCC_APB2ENR_USART1EN

// SET_BAUDRATE komutu için kullanılan başlangıç baud hızı
#define CH376_UART_BAUD                 9600U

// SET_BAUDRATE görüşmesinden sonra kullanılacak hedef baud hızı
#define CH376_UART_TARGET_BAUD          115200U

// 115200 baud için SET_BAUDRATE komutunun iki baytlık parametreleri
#define CH376_UART_TARGET_BAUD_COEF     0x03U
#define CH376_UART_TARGET_BAUD_CONST    0xCCU

// GPIO yapılandırması
#define CH376_UART_GPIO_PORT            GPIOB
#define CH376_UART_TX_PIN_NUM           6U
#define CH376_UART_RX_PIN_NUM           7U
#define CH376_UART_GPIO_AF              7U
#define CH376_UART_GPIO_AHB1_BITS       RCC_AHB1ENR_GPIOBEN

// İsteğe bağlı INT hattı
#define CH376_UART_INT_PORT             GPIOA
#define CH376_UART_INT_PIN_IDR          0xFFFFFFFFUL

#define CH376_UART_CMD_DELAY_CYCLES     64U

#endif /* UPDATER_BACKEND_CH376_UART */

#ifdef __cplusplus
}
#endif

#endif /* UPDATER_CONFIG_H */