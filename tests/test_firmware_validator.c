/**
 * @file   test_firmware_validator.c
 * @brief  updater/src/firmware_validator.c için host tarafında çalışan birim testleri.
 */

#include "unity.h"

#include <string.h>
#include <stdint.h>
#include "firmware_validator.h"
#include "memory_setup.h"
#include "updater_config.h"
#include "image.h"

#include "fake_storage_backend.h"

/* =========================================================================
 * Özel sabitler
 * ========================================================================= */
#define TEST_DATA_SIZE      UPDATER_CHUNK_SIZE
#define TEST_FILL_BYTE      0xA5U
#define TEST_MAJOR_VER      1U
#define TEST_MINOR_VER      2U
#define TEST_PATCH_VER      3U
#define TEST_GIT_SHA        "ef3b84cc"

static uint8_t gImageBuff[2560];
static StorageApi_Backend_t gFakeBackend;

/* =========================================================================
 * Özel yardımcı fonksiyonlar
 * ========================================================================= */
static uint32_t computeCRC_fake(uint8_t fillByte) {
    return ((uint32_t)fillByte) | ((uint32_t)fillByte <<  8U) 
            | ((uint32_t)fillByte << 16U) | ((uint32_t)fillByte << 24U);
}

static void buildValidImage(uint8_t *pBuff, uint32_t dataSize, uint8_t fillByte) {
    
    (void)memset(pBuff, 0x00, IMAGE_HDR_SIZE + dataSize);

    Image_Hdr_t *pHdr = (Image_Hdr_t *)pBuff;
    pHdr->image_magic       = IMAGE_MAGIC_APP;
    pHdr->image_hdr_version = IMAGE_HDR_VERSION;
    pHdr->image_type        = IMAGE_TYPE_APP;
    pHdr->version_major     = TEST_MAJOR_VER;
    pHdr->version_minor     = TEST_MINOR_VER;
    pHdr->version_patch     = TEST_PATCH_VER;
    pHdr->data_size         = dataSize;
    pHdr->crc               = computeCRC_fake(fillByte);

    (void)memcpy(pHdr->git_sha, TEST_GIT_SHA, sizeof(TEST_GIT_SHA));

    // Header sonrası data alanını doldurma
    uint8_t *pData = pBuff + IMAGE_HDR_SIZE;
    (void)memset(pData, (int)fillByte, dataSize);
}

/* =========================================================================
 * setUp / tearDown
 * ========================================================================= */
void setUp(void) {
    
    fakeStorage_reset();
    virtual_mem_set_all();

    (void)memset(gImageBuff, 0, sizeof(gImageBuff));

    FirmwareValidator_Config_t cfg;
    cfg.app_flash_base = APP_FLASH_BASE;
    cfg.app_flash_max_size = APP_FLASH_MAX_SIZE;
    firmwareValidator_setConfig(&cfg);
}

void tearDown(void) {
    // Hiçbir şey yapmama
}

/* =========================================================================
 * Testler
 * ========================================================================= */
/**
  * @brief Mutlu senaryo: Doğru magic, tür, boyut ve CRC değerlerine sahip geçerli bir imaj
  */
void test_firmwareValidator_validImage_returnsOk(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);
    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_OK, res);
}

/**
  * @brief  NULL backend işaretçisi derhal READ_ERROR döndürmelidir
  */
void test_firmwareValidator_nullBackend_returnsReadError(void) {
    FirmwareValidator_Result_e res = firmwareValidator_run(NULL, NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_READ_ERROR, res);
}

/**
 * @brief Yanlış image_magic değeri BAD_MAGIC döndürmelidir
 */
void test_firmwareValidator_badMagic_returnsBadMagic(void) {
    
    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);

    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->image_magic = 0xDEADBEEFU;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_BAD_MAGIC, res);
}

/**
 * @brief Updater magic (app magic yerine) BAD_MAGIC döndürmelidir
 */
void test_firmwareValidator_updaterMagic_returnsBadMagic(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);
    
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->image_magic = IMAGE_MAGIC_UPDATER;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_BAD_MAGIC, res);
}

/**
 * @brief IMAGE_TYPE_UPDATER (IMAGE_TYPE_APP yerine) BAD_TYPE döndürmelidir
 */
void test_firmwareValidator_wrongImageType_returnsBadType(void) {
    
    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);
    
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->image_type = IMAGE_TYPE_UPDATER;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_BAD_TYPE, res);
}

/**
 * @brief data_size == 0 ise BAD_SIZE döndürülmelidir
 */
void test_firmwareValidator_zeroDataSize_returnsBadSize(void) {
    
    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);
    
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->data_size = 0;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_BAD_SIZE, res);
}

/**
 * @brief data_size mevcüt maksimum alandan büyük ise BAD_SIZE döndürülmelidir
 */
void test_firmwareValidator_oversizedData_returnsBadSize(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);
    
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->data_size = APP_FLASH_MAX_SIZE + 1;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_BAD_SIZE, res);
}

/**
 * @brief Bozuk CRC alanı BAD_CRC döndürmelidir
 */
void test_firmwareValidator_crcMismatch_returnsBadCrc(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);
    
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;
    pHdr->crc = 0xFFFFFFFFU;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_BAD_CRC, res);
}

/**
 * @brief Veri akışı aşamasında 0 bayt döndüren depolama birimi READ_ERROR döndürmelidir
 * @details Fake backend'e sadece başlık yerleştirilir. Böylece başlık okuma başarılı 
 *          olup ilk veri parçası okuması başarısız kalır ve got = 0 olur.
 */
void test_firmwareValidator_dataReadFail_returnsReadError(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);
    
    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_READ_ERROR, res);
}

/**
 * @brief Başarı durumunda FirmwareValidator_Info_t tipinde olan yapının 
 *      bütün alanları header'daki verilerle doldurulur
 */
void test_firmwareValidator_infoFilled_onSuccess(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);

    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Info_t info;
    (void)memset(&info, 0x00, sizeof(info));

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), &info);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_OK, res);
    TEST_ASSERT_EQUAL_UINT32(pHdr->data_size, info.data_size);
    TEST_ASSERT_EQUAL_UINT32(computeCRC_fake(TEST_FILL_BYTE), info.stored_crc);
    TEST_ASSERT_EQUAL_UINT32(computeCRC_fake(TEST_FILL_BYTE), info.computed_crc);
    TEST_ASSERT_EQUAL_UINT8(pHdr->version_major, info.major_ver);
    TEST_ASSERT_EQUAL_UINT8(pHdr->version_minor, info.minor_ver);
    TEST_ASSERT_EQUAL_UINT8(pHdr->version_patch, info.patch_ver);
    TEST_ASSERT_EQUAL_STRING(pHdr->git_sha, info.git_sha);
}

/**
 * @brief Backend'deki depolama birimi NULL olmayıp FirmwareValidator_Info_t yerine NULL 
 *      işaretçisi kullanıldığında `firmwareValidator_run()` akışı bozulmamalıdır
 */
void test_firmwareValidator_nullInfo_noCrash(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);

    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + TEST_DATA_SIZE);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_OK, res);
}

/**
 * @brief data_size'ın TEST_DATA_SIZE'dan büyük olması, birden fazla `fileRead()` çağrısı 
 *      yapılmasını zorlar. Doğrulayıcının buna rağmen başarılı olması gerekir.
 */
void test_firmwareValidator_continuousData_crcCorrect(void) {

    uint32_t dataSize = 3 * TEST_DATA_SIZE;
    uint8_t fillByte = 0xABU;
    buildValidImage(gImageBuff, dataSize, fillByte);
    
    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE + dataSize);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_OK, res);
}

/**
 * @brief `fileRead()` header için gerektiğinden daha az bayt sayısı okuduğu için 
 *      READ_ERROR gönderilmelidir.
 */
void test_firmwareValidator_shortHeaderRead_returnsReadError(void) {

    buildValidImage(gImageBuff, TEST_DATA_SIZE, TEST_FILL_BYTE);

    Image_Hdr_t *pHdr = (Image_Hdr_t *)gImageBuff;

    fakeStorage_setData(gImageBuff, IMAGE_HDR_SIZE - 1);

    FirmwareValidator_Result_e res = firmwareValidator_run(fakeStorage_getBackend(), NULL);

    TEST_ASSERT_EQUAL_INT(FIRMWARE_VALIDATOR_RESULT_READ_ERROR, res);
}

/**
 * @brief NULL işaretçisi ile setConfig işlevi çökmemelidir
 */
void test_firmwareValidator_setConfigNull_noCrash(void) {

    firmwareValidator_setConfig(NULL);
    // Buraya kadar gelebildi ise demek ki `firmwareValidator_setConfig()` NULL işaretçisinden dolayı çökmedi
    TEST_PASS();
}