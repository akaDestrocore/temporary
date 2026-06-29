#include "fake_storage_backend.h"

typedef struct {
    const uint8_t *pData;   // imaj işaretçisi
    size_t dataSize;        // imajın toplam bayt sayısı
    size_t readOffset;      // her `fileRead()` çağrısında okunan bayt kursörü
} FakeStorage_State_t;
static FakeStorage_State_t g_state;

/* =========================================================================
 * Vektör tablosunun uygulanması
 * ========================================================================= */
/**
 * @brief Fake init - Her zaman STORAGE_STATUS_OK döndürür.
 * @retval STORAGE_STATUS_OK
 */
static StorageApi_Status_e fakeInit(void) {
    return STORAGE_STATUS_OK;
}

/**
 * @brief Fake deinit - Hiçbir işlem yapılmaz
 * @retval None
 */
static void fakeDeinit(void) {
    // Hiçbir işlem yapma
}

/**
 * @brief Fake isReady - USB her zaman bağlı olarak kabul edilir.
 * @retval STORAGE_STATUS_OK
 */
static StorageApi_Status_e fakeIsReady(void) {
    return STORAGE_STATUS_OK;
}

/**
 * @brief Fake fileOpen - Herhangi bir dosya açılır ve okuma offseti sıfırlanır.
 * @param pPath Dosya yolu
 * @retval STORAGE_STATUS_OK
 */
static StorageApi_Status_e fakeFileOpen(const char *pPath) {
    (void)pPath;
    g_state.readOffset = 0;
    return STORAGE_STATUS_OK;
}

/**
 * @brief Fake fileClose - Her zaman STORAGE_STATUS_OK döndürür.
 * @retval STORAGE_STATUS_OK
 */
static StorageApi_Status_e fakeFileClose(void) { 
    return STORAGE_STATUS_OK;
}

/**
 * @brief Fake fileRead - Verilen buffer boyutuna kadar veri okur.
 * @param pBuff Hedef buffer.
 * @param buffSize Okunacak maksimum byte sayısı.
 * @param pBytesRead Gerçekten okunan veri boyutu.
 * @retval STORAGE_STATUS_OK
 */
static StorageApi_Status_e fakeFileRead(uint8_t *pBuff, uint32_t buffSize, uint32_t *pBytesRead) {
    
    *pBytesRead = 0;

    if (NULL == g_state.pData) {
        return STORAGE_STATUS_READ_ERROR;
    }

    size_t remaining = g_state.dataSize - g_state.readOffset;
    size_t toCopy = (size_t)buffSize < remaining ? (size_t)buffSize : remaining;

    (void)memcpy(pBuff, g_state.pData + g_state.readOffset, toCopy);
    g_state.readOffset += toCopy;
    *pBytesRead = (uint32_t)toCopy;

    return STORAGE_STATUS_OK;
}

static const StorageApi_Backend_t gFakeBackend = {
    .init = fakeInit,
    .deinit = fakeDeinit,
    .isReady = fakeIsReady,
    .fileOpen = fakeFileOpen,
    .fileClose = fakeFileClose,
    .fileRead = fakeFileRead,
};

/**
  * @brief Test edilen modüle eklenmek üzere fake backend vektör 
  *       tablosunu döndürür.
  * @retval Doldurulmuş bir StorageApi_Backend_t yapısına işaretçi.
  */
const StorageApi_Backend_t *fakeStorage_getBackend(void) {
    return &gFakeBackend;
}

/**
 * @brief Fake backend durumunu sıfırlar
 * @retval None
 */
void fakeStorage_reset(void) {
    (void)memset(&g_state, 0U, sizeof(g_state));
}

/**
  * @brief  fileRead üzerinden akış olarak okunacak bir imaj tamponunu hazırlar.
  * @param pData Akış olarak okunacak imaj verisine işaretçi
  * @param size İmajın toplam bayt sayısı
  * @retval None
  */
void fakeStorage_setData(const uint8_t *pData, size_t size) {
    g_state.pData = pData;
    g_state.dataSize = size;
    g_state.readOffset = 0U;
}