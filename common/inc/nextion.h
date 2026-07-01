/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 *
 * @file    nextion.h
 * @brief   Doğrudan register tabanlı, kesme tabanlı Nextion HMI ekran sürücüsü.
 *
 * @author  destrocore
 * @date    2026
 *
 * @details
 */

#ifndef _NEXTION_H
#define _NEXTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f407xx.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * Ring buffer ayarları
 * ============================================================*/
#define NEXTION_RING_BUFF_SIZE      256U
#define NEXTION_CMD_QUEUE_DEPTH     8U
#define NEXTION_CMD_MAX_LEN         64U
#define NEXTION_FRAME_QUEUE_DEPTH   8U
#define NEXTION_RX_FRAME_MAX_LEN    256U
#define NEXTION_TERM_BYTE           0xFFU
#define NEXTION_TERM_LEN            3U

/* ============================================================
 * Durum kodları
 * ============================================================*/
typedef enum {
    NEXTION_STATUS_OK               = 0,
    NEXTION_STATUS_BUSY             = -1,
    NEXTION_STATUS_INVALID_PARAM    = -2,
    NEXTION_STATUS_QUEUE_FULL       = -3,
    NEXTION_STATUS_TIMEOUT          = -4,
    NEXTION_STATUS_ERROR            = -5,
    NEXTION_STATUS_EMPTY            = -6
} Nextion_Status_e;

/* ============================================================
 * Nextion yanıt kodları (bkcmd=3 modu)
 * ============================================================*/
typedef enum {
    NEXTION_RESP_INVALID_INSTR      = 0x00U,    // Invalid instruction
    NEXTION_RESP_SUCCESS            = 0x01U,    // Instruction Successful
    NEXTION_RESP_INVALID_CMP_ID     = 0x02U,    // Invalid Component ID
    NEXTION_RESP_INVALID_PAGE_ID    = 0x03U,    // Invalid Page ID
    NEXTION_RESP_INVALID_PIC_ID     = 0x04U,    // Invalid Picture ID
    NEXTION_RESP_INVALID_FONT_ID    = 0x05U,    // Invalid Font ID
    NEXTION_RESP_INVALID_FILE_OP    = 0x06U,    // Invalid File Operation
    NEXTION_RESP_INVALID_CRC        = 0x09U,    // Invalid CRC
    NEXTION_RESP_INVALID_BAUD       = 0x11U,    // Invalid Baud rate Setting
    NEXTION_RESP_INVALID_WAVEFORM   = 0x12U,    // Invalid Waveform ID or Channel #
    NEXTION_RESP_INVALID_VAR        = 0x1AU,    // Invalid Variable name or attribute
    NEXTION_RESP_INVALID_VAR_OP     = 0x1BU,    // Invalid Variable Operation
    NEXTION_RESP_ASSIGN_FAIL        = 0x1CU,    // Assignment failed to assign
    NEXTION_RESP_EEPROM_FAIL        = 0x1DU,    // EEPROM Operation failed
    NEXTION_RESP_PARAM_CNT_INVALID  = 0x1EU,    // Invalid Quantity of Parameters
    NEXTION_RESP_PARAM_IO_FAIL      = 0x1FU,    // IO Operation failed
    NEXTION_RESP_ESCAPE_INVALID     = 0x20U,    // Escape Character Invalid
    NEXTION_RESP_VAR_TOO_LONG       = 0x23U,    // Variable name too long
    NEXTION_RESP_SERIAL_BUF_OVF     = 0x24U     // Serial Buffer Overflow
} Nextion_Resp_e;

/* ==============================================================
 * Sürücü durum makinesi
 * =============================================================*/
typedef enum {
    NEXTION_STATE_IDLE      = 0U,
    NEXTION_STATE_SENDING   = 1U,
    NEXTION_STATE_WAIT_ACK  = 2U,
    NEXTION_STATE_TIMEOUT   = 3U,
    NEXTION_STATE_FAULTED   = 4U
} Nextion_State_e;

/* ==============================================================
 * Frame tipleri
 * =============================================================*/

 typedef enum {
    NEXTION_FRAME_ACK       = 0U,
    NEXTION_FRAME_TOUCH     = 1U,
    NEXTION_FRAME_PAGE      = 3U,
    NEXTION_FRAME_NUMERIC   = 4U,
    NEXTION_FRAME_STRING    = 5U,
    NEXTION_FRAME_EVENT     = 6U,
    NEXTION_FRAME_UNKNOWN   = 7U
} Nextion_FrameType_e;

typedef struct {
    Nextion_FrameType_e type;
    uint8_t code;
    uint16_t length;
    uint8_t data[NEXTION_RX_FRAME_MAX_LEN];
} Nextion_Frame_t;

/**
 * @brief Nextion UART için yapılandırma yapısı
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
    uint32_t baud_rate;
    IRQn_Type irqn;
    uint32_t nvic_priority;
    uint32_t ack_timeout_ms;
} Nextion_Config_t;

/* =========================================================================
 * Genel API
 * ========================================================================= */

/**
 * @brief GPIO, USART çevre birimini, NVIC başlatır ve bkcmd=3 sıraya alır
 * @param pCfg Tamamen doldurulmuş bir Nextion_Config_t yapısına işaretçi
 * @retval Başarılı olursa `NEXTION_STATUS_OK` veya negatif bir hata kodu
 */
Nextion_Status_e nextion_init(const Nextion_Config_t *pCfg);

/**
 * @brief İletim için bir Nextion komut buffer'ını sıraya ekler
 * @param pCmd NULL karakter ile sonlandırılmış komut stringi
 * @retval Başarıyla sıraya eklendiyse `NEXTION_STATUS_OK`
 */
Nextion_Status_e nextion_sendCmd(char *pCmd);

/**
 * @brief Sürücünün bekleyen herhangi bir işi olup olmadığını döndürür
 * @retval Sürücü IDLE durumunda değilse veya komut kuyruğu boş değilse `true`
 */
bool nextion_isBusy(void);

/**
 * @brief Geçerli durum makinesi durumunu döndürür
 * @retval Geçerli Nextion_State_e değeri
 */
Nextion_State_e nextion_getState(void);

/**
 * @brief Son ACK veya hata yanıt kodunu döndürür
 * @retval Son Nextion yanıt kodu
 */
uint8_t nextion_getLastResponse(void);

/**
 * @brief Son tamamlanan komutun sonucunu döndürür
 * @retval Komut durumu
 */
Nextion_Status_e nextion_getLastCommandStatus(void);

/**
 * @brief Sıradaki alınmış Nextion frame'ini döndürür
 * @param pFrame Çıkış frame yapısı
 * @retval Frame varsa `NEXTION_STATUS_OK`, yoksa `NEXTION_STATUS_EMPTY`
 */
Nextion_Status_e nextion_readFrame(Nextion_Frame_t *pFrame);

/**
 * @brief Nextion durum makinesini yönetir
 * @retval None
 */
void nextion_process(void);

/**
 * @brief USART kesme işleyicisi
 * @retval None
 */
void nextion_irqHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* _NEXTION_H */