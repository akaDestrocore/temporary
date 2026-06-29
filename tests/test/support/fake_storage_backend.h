/**
 * @file   fake_storage_backend.h
 * @brief  Birim testleri için fake StorageApi_Backend_t uygulaması.
 *
 * @details Bu uygulama, `firmware_validator.c` ve `updater.c` dosyalarının host 
 *    ortamındaki testleri sırasında gerçek depolama backend`i yerine kullanılır.
 *    Kullanım şekli:
 *      1. `setUp()` içerisinde `fakeStorage_reset()` çağırın.
 *      2. Bir firmware imaj buffer oluşturun (başına Image_Hdr_t eklenmiş şekilde).
 *      3. Bu buffer ile `fakeStorage_setData()` çağırın.
 *      4. `fakeStorage_getBackend()` ile backend işaretçisini alın ve test edilen
 *        fonksiyona aktarın.
 */

#ifndef FAKE_STORAGE_BACKEND_H
#define FAKE_STORAGE_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "storage_api.h"
#include <stdint.h>
#include <stddef.h>

/**
  * @brief Tüm fake durumları temizler.
  * @retval None
  * @details `setUp()` fonksiyonunda çağrılmalıdır.
  */
void fakeStorage_reset(void);

/**
  * @brief  fileRead üzerinden akış olarak okunacak bir imaj tamponunu hazırlar.
  * @param pData Akış olarak okunacak imaj verisine işaretçi
  * @param size İmajın toplam bayt sayısı
  * @retval None
  */
void fakeStorage_setData(const uint8_t *pData, size_t size);

/**
  * @brief Test edilen modüle eklenmek üzere fake backend vektör 
  *       tablosunu döndürür.
  * @retval Doldurulmuş bir StorageApi_Backend_t yapısına işaretçi.
  */
const StorageApi_Backend_t *fakeStorage_getBackend(void);

#ifdef __cplusplus
}
#endif

#endif // FAKE_STORAGE_BACKEND_H