/**
 * @file   test_nextion.c
 * @brief  common/src/nextion.c için host tarafında çalışan birim testleri.
 *
 * @details RX baytları SR/DR üzerinden yazılıp `nextion_irqHandler()` çağrılarak 
 *          beslenir; TX baytları aynı şekilde TXE bayrağı setlenip 
 *          `nextion_irqHandler()` çağrılarak DR'den okunarak yakalanır.
 */

#include "unity.h"

#include <string.h>
#include <stdint.h>

#include "nextion.h"
#include "memory_setup.h"
#include "mock_stm32f4xx_hal.h"


/* =========================================================================
 * Özel sabitler
 * ========================================================================= */

static Nextion_Config_t gTestCfg;
#define TEST_ACK_TIMEOUT_MS 250U


/* =========================================================================
 * Özel yardımcı fonksiyonlar
 * ========================================================================= */

/**
 * @brief USART üzerinden tek bir RX baytını sürücüye besler
 * @param byte Alınan veri baytı
 * @retval None
 */
static void simRxByte(uint8_t byte) {
    
    USART3->DR = (uint32_t)byte;
    USART3->SR |= USART_SR_RXNE;

    nextion_irqHandler();
}

/**
 * @brief Bir RX frame'ini sürücüye besler
 * @param pPayload Frame'ın verisi
 * @param len Payload uzunluğu
 * @retval None
 */
static void simRxFrame(const uint8_t *pPayload, uint32_t len) {
    
    uint32_t i;
    for (i = 0U; i < len; i++) {
        simRxByte(pPayload[i]);
    }

    simRxByte(NEXTION_TERM_BYTE);
    simRxByte(NEXTION_TERM_BYTE);
    simRxByte(NEXTION_TERM_BYTE);
}

/**
  * @brief Ön sıradaki komutun iletimini TXEIE temizlenene kadar sürer ve 
  *         iletilen baytları yakalar.
  * @param pOut Yakalanan baytlar için çıkış buffer'ı
  * @param maxLen pOut buffer'ın kapasitesi
  * @retval Yakalanan bayt sayısı
  */
static uint32_t simDrainTx(uint8_t *pOut, uint32_t maxLen) {
    
    uint32_t n = 0U;

    while (0U != (USART3->CR1 & USART_CR1_TXEIE)) {
        
        USART3->SR |= USART_SR_TXE;
        nextion_irqHandler(); 

        // TXEIE setliyse bu çağrıda bir bayt iletildi anlamına gelir
        if (0U != (USART3->CR1 & USART_CR1_TXEIE)) {
            if (n < maxLen) {
                pOut[n] = (uint8_t)(USART3->DR && 0xFFU);
            }
            n++;
        }
    }

    // TX kesmesi
    USART3->SR |= USART_SR_TC;
    nextion_irqHandler();
    
    return n;
}

/**
 * @brief Ön sıradaki komutu IDLE -> SENDING durumuna geçirir
 * @param pOut Yakalanan baytlar için çıkış buffer'ı
 * @param maxLen pOut buffer'ın boyutu
 * @retval Yakalanan bayt sayısı
 */
static uint32_t simTransmit(uint8_t *pOut, uint32_t maxLen) {
    
    nextion_process();

    return simDrainTx(pOut, maxLen);
}

/**
 * @brief Bir komutu sıraya alır, iletimi sürer, başarılı ACK simüle eder 
 *      ve IDLE durumuna geçirir.
 * @param pCmd Gönderilecek komut
 * @param pOut Yakalanan TX baytlar için çıkış buffer'ı
 * @param maxLen pOut buffer'ın boyutu
 * @retval Yakalanan TX bayt sayısı
 */
static uint32_t simSendCmd(char *pCmd, uint8_t *pOut, uint32_t maxLen) {

    Nextion_Status_e sendStatus = nextion_sendCmd(pCmd);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatus);

    uint32_t txLen = simTransmit(pOut, maxLen);

    uint8_t ack[1] = {(uint8_t)NEXTION_RESP_SUCCESS};
    simRxFrame(ack, 1);
    nextion_process();

    return txLen;
}

/* =========================================================================
 * setUp / tearDown
 * ========================================================================= */

void setUp(void) {
    
    virtual_mem_set_all();

    HAL_GetTick_IgnoreAndReturn(0U);
    (void)memset(&gTestCfg, 0x00, sizeof(gTestCfg));
    gTestCfg.pUsart = USART3;
    gTestCfg.pGpio_port = GPIOB;
    gTestCfg.rx_pin_num = 11U;
    gTestCfg.tx_pin_num = 10U;
    gTestCfg.gpio_af = 7U;
    gTestCfg.gpio_ahb1_bits = RCC_AHB1ENR_GPIOBEN;
    gTestCfg.apbx_bit = RCC_APB1ENR_USART3EN;
    gTestCfg.is_apb2 = 0U;
    gTestCfg.apb_freq_hz = 48000000U;
    gTestCfg.baud_rate = 9600U;
    gTestCfg.irqn = USART3_IRQn;
    gTestCfg.nvic_priority = 8U;
    gTestCfg.ack_timeout_ms = TEST_ACK_TIMEOUT_MS;

    Nextion_Status_e initStatus = nextion_init(&gTestCfg);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, initStatus);

    uint8_t txBuff[16];
    uint32_t txBuffLen = simTransmit(txBuff, sizeof(txBuff));
    // bkcmd=3 + 0xFF 0xFF 0xFF
    TEST_ASSERT_EQUAL_UINT32(11U, txBuffLen);

    uint8_t ack[1] = {(uint8_t)NEXTION_RESP_SUCCESS};
    simRxFrame(ack, 1);
    nextion_process();

    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_IDLE, nextion_getState());
    TEST_ASSERT_FALSE(nextion_isBusy());
}

void tearDown(void) {
    // Hiçbir şey yapmama
}

/* =========================================================================
 * Testler
 * ========================================================================= */

/**
 * @brief 
 */
void test_nextion_init_nullConfig_returnsInvalidParam(void) {
    Nextion_State_e status = nextion_init(NULL);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_INVALID_PARAM, status);
}