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
static uint32_t gFakeTickMs = 0U;

#define TEST_ACK_TIMEOUT_MS 250U



/* =========================================================================
 * Özel yardımcı fonksiyonlar
 * ========================================================================= */

/**
  * @brief HAL_GetTick() için CMock geri çağırımı
  * @param numCalls CMock'un sağladığı çağrı sayacı (kullanılmıyor)
  * @retval Ayarlanmış sahte zaman değeri
  */
static uint32_t cbHalGetTick(int numCalls) {
    (void)numCalls;
    return gFakeTickMs;
}

/**
 * @brief USART üzerinden tek bir RX baytını sürücüye besler
 * @param byte Alınan veri baytı
 * @retval None
 */
static void simRxByte(uint8_t byte) {
    
    USART3->DR = (uint32_t)byte;
    USART3->SR |= USART_SR_RXNE;

    nextion_irqHandler();

    USART3->SR &= ~USART_SR_RXNE;
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
                pOut[n] = (uint8_t)(USART3->DR & 0xFFU);
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

    gFakeTickMs = 0U;
    HAL_GetTick_StubWithCallback(cbHalGetTick);

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
    TEST_ASSERT_EQUAL_UINT32(10U, txBuffLen);

    uint8_t ack[1] = {(uint8_t)NEXTION_RESP_SUCCESS};
    simRxFrame(ack, 1);
    nextion_process();

    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_IDLE, nextion_getState());
    TEST_ASSERT_FALSE(nextion_isBusy());

    // Önce bkcmd=3 tüketilmeli
    Nextion_Frame_t frame;
    Nextion_Status_e readStatus = NEXTION_STATUS_ERROR;
    readStatus = nextion_readFrame(&frame);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, readStatus);
    TEST_ASSERT_EQUAL_INT(NEXTION_FRAME_ACK, frame.type);
    TEST_ASSERT_EQUAL_UINT8(NEXTION_RESP_SUCCESS, frame.code);
}

void tearDown(void) {
    // Hiçbir şey yapmama
}

/* =========================================================================
 * Testler
 * ========================================================================= */

/**
 * @brief NULL yapılandırma ile init INVALID_PARAM döndürmelidir
 */
void test_nextion_init_nullConfig_returnsInvalidParam(void) {
    Nextion_Status_e status = nextion_init(NULL);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_INVALID_PARAM, status);
}

/**
  * @brief Gönderilen komutun TX baytları ve sıralaması doğru olmalıdır
  */
void test_nextion_sendCmd_txBytesAndOrder_correct(void) {
    
    uint8_t txBuff[32];

    Nextion_Status_e sendStatus = nextion_sendCmd("j0.val=50");
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatus);

    uint32_t txLen = simTransmit(txBuff, sizeof(txBuff));

    TEST_ASSERT_EQUAL_UINT32(12U, txLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)"j0.val=50", txBuff, 9);
    TEST_ASSERT_EQUAL_UINT8(NEXTION_TERM_BYTE, txBuff[9]);
    TEST_ASSERT_EQUAL_UINT8(NEXTION_TERM_BYTE, txBuff[10]);
    TEST_ASSERT_EQUAL_UINT8(NEXTION_TERM_BYTE, txBuff[11]);
}

/**
 * @brief NEXTION_CMD_MAX_LEN uzunluğundan fazla bir komut reddedilmelidir.
 */
void test_nextion_sendCmd_overLengthReject(void) {
    
    char oversized[NEXTION_CMD_MAX_LEN + 2U];
    uint32_t i;

    for (i = 0U; i < (NEXTION_CMD_MAX_LEN + 1U); i++) {
        oversized[i] = 'n';
    }
    oversized[NEXTION_CMD_MAX_LEN + 1U] = '\0';

    Nextion_Status_e sendStatus = nextion_sendCmd(oversized);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_QUEUE_FULL, sendStatus);
}

/**
 * @brief Komut kuyruğu dolduğunda ek bir gönderim QUEUE_FULL döndürmelidir
 */
void test_nextion_sendCmd_queueFullReject(void) {

    uint32_t slots = NEXTION_CMD_QUEUE_DEPTH - 1U;
    uint32_t i;
    Nextion_Status_e sendStatus;

    for (i = 0U; i < slots; i++) {
        sendStatus = nextion_sendCmd("t0.txt=\"test\"");
        TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatus);
    }

    sendStatus = nextion_sendCmd("t0.txt=\"test overflow\"");
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_QUEUE_FULL, sendStatus);
}

/**
 * @brief nextion_isBusy() durum geçişleri boyunca doğru değeri döndürmelidir
 */
void test_nextion_isBusy_betweenStates(void) {
    
    uint8_t txBuff[32];

    // IDLE
    TEST_ASSERT_FALSE(nextion_isBusy());

    Nextion_Status_e sendStatus = nextion_sendCmd("j0.val=1");
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatus);
    TEST_ASSERT_TRUE(nextion_isBusy());

    (void)simTransmit(txBuff, sizeof(txBuff));
    (void)nextion_process();
    // TX bitti ama ACK henüz gelmedi -> WAIT_ACK hala meşgul
    TEST_ASSERT_TRUE(nextion_isBusy());
    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_WAIT_ACK, nextion_getState());

    uint8_t ack[1] = {(uint8_t)NEXTION_RESP_SUCCESS};
    simRxFrame(ack, 1);
    (void)nextion_process();

    TEST_ASSERT_FALSE(nextion_isBusy());
    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_IDLE, nextion_getState());
}

/**
 * @brief Birden fazla sıraya alınmış komut FIFO sırasıyla iletilmelidir
 */
void test_nextion_multipleCmds_fifoOrder(void) {
    
    uint8_t txBuffA[32];
    uint8_t txBuffB[32];

    Nextion_Status_e sendStatusA = nextion_sendCmd("t0.txt=\"A\"");
    Nextion_Status_e sendStatusB = nextion_sendCmd("t0.txt=\"B\"");
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatusA);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatusB);

    uint32_t lenA = simTransmit(txBuffA, sizeof(txBuffA));
    TEST_ASSERT_EQUAL_UINT32(13U, lenA);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)"t0.txt=\"A\"", txBuffA, 10U);

    uint8_t ack[1] = {(uint8_t)NEXTION_RESP_SUCCESS};
    simRxFrame(ack, 1);
    nextion_process();

    uint32_t lenB = simTransmit(txBuffB, sizeof(txBuffB));
    TEST_ASSERT_EQUAL_UINT32(13U, lenB);
    TEST_ASSERT_EQUAL_UINT8_ARRAY((const uint8_t *)"t0.txt=\"B\"", txBuffB, 10U);

    simRxFrame(ack, 1);
    nextion_process();

    TEST_ASSERT_FALSE(nextion_isBusy());
}

/**
 * @brief Hata koduyla gelen bir ACK, komut durumunu FAULTED olarak
 *      işaretlemeli ve IDLE'a geri dönmelidir
 */
void test_nextion_errorAck_setsErrorStatus(void) {

    uint8_t txBuff[32];

    Nextion_Status_e sendStatus = nextion_sendCmd("j0.val=99");
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatus);
    (void)simTransmit(txBuff, sizeof(txBuff));

    uint8_t badAck[1] = {(uint8_t)NEXTION_RESP_INVALID_CMP_ID};
    simRxFrame(badAck, 1U);
    nextion_process();

    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_ERROR, nextion_getLastCommandStatus());
    TEST_ASSERT_EQUAL_UINT8(NEXTION_RESP_INVALID_CMP_ID, nextion_getLastResponse());
    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_FAULTED, nextion_getState());

    nextion_process();
    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_IDLE, nextion_getState());
}

/**
 * @brief ACK zaman aşımına uğradığında sürücü TIMEOUT durumuna geçmeli ve 
 *      ardından IDLE'a dönmelidir
 */
void test_nextion_ackTimeout_setsTimeoutStatus(void) {

    uint8_t txBuff[32];

    Nextion_Status_e sendStatus = nextion_sendCmd("j0.val=1");
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, sendStatus);
    (void)simTransmit(txBuff, sizeof(txBuff));
    nextion_process();

    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_WAIT_ACK, nextion_getState());

    gFakeTickMs = TEST_ACK_TIMEOUT_MS + 1U;
    nextion_process();

    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_TIMEOUT, nextion_getLastCommandStatus());
    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_TIMEOUT, nextion_getState());

    nextion_process();
    TEST_ASSERT_EQUAL_INT(NEXTION_STATE_IDLE,nextion_getState());
    TEST_ASSERT_FALSE(nextion_isBusy());
}

/**
 * @brief Bir touch frame'i doğru şekilde ayrıştırılmalı
 */
void test_nextion_rxTouchFrame_parsedCorrectly(void) {

    uint8_t touchPayload[4] = {0x65U, 0x00U, 0x01U, 0x01U};

    simRxFrame(touchPayload, sizeof(touchPayload));
    nextion_process();

    Nextion_Frame_t frame;
    Nextion_Status_e readStatus = nextion_readFrame(&frame);

    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, readStatus);
    TEST_ASSERT_EQUAL_INT(NEXTION_FRAME_TOUCH, frame.type);
    TEST_ASSERT_EQUAL_UINT8(0x65U, frame.code);
    TEST_ASSERT_EQUAL_UINT16(3U, frame.length);
    TEST_ASSERT_EQUAL_UINT8(0x00U, frame.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, frame.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, frame.data[2]);

    readStatus = nextion_readFrame(&frame);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_EMPTY, readStatus);
}

/**
 * @brief Frame kuyruğu dolduğunda en eski frame'ler atılmalı, en yeni olanlar korunmalıdır
 */
void test_nextion_frameQueueOverflow_fifoTest(void) {

    uint32_t pushedFrameCount = NEXTION_FRAME_QUEUE_DEPTH + 3U;
    uint32_t i;

    for (i = 0U; i < pushedFrameCount; i++) {
        uint8_t payload[1];
        payload[0] = (uint8_t)(0x5AU + i);
        simRxFrame(payload, sizeof(payload));
    }
    nextion_process();

    // Sıra taştıktan sonra ilk gelen frame
    uint32_t expectedInQueue = NEXTION_FRAME_QUEUE_DEPTH - 1U;
    uint32_t firstExpectedCode = 0x5AU + (pushedFrameCount - expectedInQueue);

    Nextion_Frame_t frame;
    for (i = 0U; i < expectedInQueue; i++) {
        Nextion_Status_e readStatus = nextion_readFrame(&frame);
        TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_OK, readStatus);
        TEST_ASSERT_EQUAL_INT(NEXTION_FRAME_EVENT, frame.type);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(firstExpectedCode + i), frame.code);
    }

    Nextion_Status_e readStatus = nextion_readFrame(&frame);
    TEST_ASSERT_EQUAL_INT(NEXTION_STATUS_EMPTY, readStatus);
}