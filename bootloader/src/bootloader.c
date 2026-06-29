/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file            bootloader.c
 * @brief           16 KB'lık alana sığacak şekilde tasarlanmış, donanıma doğrudan 
 *                  erişim sağlayan önyükleyici.
 * 
 * @author          destrocore
 * @date            2026
 * 
 * @details
 * Sistem ilklendirme, CRC32 kullanarak firmware imajı doğrulama ve cihaz 
 * uygulama imajına güvenli geçiş işlemlerini gerçekleştirir.
 */

#include "main.h"

/* ============================================================
 * Özel tip tanımlamaları
 * ============================================================*/
typedef enum {
    BL_BOOTTARGET_NONE      = 0U,
    BL_BOOTTARGET_UPDATER   = 1U,
    BL_BOOTTARGET_APP       = 2U
} bl_bootTarget_e;

/** @addtogroup Exported_macro
  * @{
  */
#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#define READ_BIT(REG, BIT)    ((REG) & (BIT))

#define CLEAR_REG(REG)        ((REG) = (0x0))

#define WRITE_REG(REG, VAL)   ((REG) = (VAL))

/* ============================================================
 * Özel fonksiyon prototipleri
 * ============================================================*/
static void rcc_init(void);
static uint32_t crc_calculate(uint32_t addr, uint32_t size);
bool bl_isImageValid(uint32_t imageAddr, uint32_t imageMagic);
static bl_bootTarget_e bl_getBootTarget(void);
static void deinit_system(void);
static __attribute__((noreturn)) void boot_to_image(uint32_t vectorAddr);
void HardFault_Handler(void);

/**
  * @brief  Önyükleyicinin ana giriş noktası
  * @retval int Normal çalışmada asla geri dönmez
  */
int main(void) {
    
    rcc_init();

    const bl_bootTarget_e target = bl_getBootTarget();

    switch(target) {
        case BL_BOOTTARGET_APP: {
            deinit_system();
            boot_to_image(APP_ADDR + IMAGE_HDR_SIZE);
            break;
        }

        case BL_BOOTTARGET_UPDATER: {
            deinit_system();
            boot_to_image(UPDATER_ADDR + IMAGE_HDR_SIZE);
            break;
        }

        default: {
            break;
        }
    }

    while (1) {
        __NOP();
    }

}

/**
  * @brief  RCC saat sistemini HSI olarak başlat
  * @retval None
  */
static void rcc_init(void) {
    
    RCC->CR |= RCC_CR_HSION;
    while (0U == (RCC->CR & RCC_CR_HSIRDY)) {
        // HSI'nin hazır olmasını bekle
    }

    // SYSCLK'u HSI'ye değiştirin
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while (RCC_CFGR_SWS_HSI != (RCC->CFGR & RCC_CFGR_SWS)) {
        // HSI bitinin ayarlanmasını bekle
    }

    // Çevre birimlerinin saatlerini etkinleştirin
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN | RCC_AHB1ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;
}

/**
 * @brief Hangi bileşenin başlatılacağını belirler
 * @retval `bl_bootTarget_e` Başlatılacak hedef veya her 
 * ikisi de başarısız olursa `BL_BOOTTARGET_NONE`
 */
static bl_bootTarget_e bl_getBootTarget(void) {

    if (true == bl_isImageValid(APP_ADDR, IMAGE_MAGIC_APP)) {
        return BL_BOOTTARGET_APP;
    }

    if (true == bl_isImageValid(UPDATER_ADDR, IMAGE_MAGIC_UPDATER)) {
        return BL_BOOTTARGET_UPDATER;
    }

    return BL_BOOTTARGET_NONE;
}

/**
 * @brief Bir firmware imajını doğrular
 * @param imageAddr İmajının Image_Hdr_t'li flash adresi
 * @param imageMagic Image_Hdr_t.image_magic'te beklenen magic değer
 * @retval İmaj başlığı ve CRC geçerliyse - `true`
 */
bool bl_isImageValid(uint32_t imageAddr, uint32_t imageMagic) {

    const Image_Hdr_t *pHdr = (const Image_Hdr_t *)imageAddr;
    
    if (pHdr->image_magic != imageMagic) {
        return false;
    }

    if (0U == pHdr->data_size) {
        return false;
    }

    const uint32_t fwAddr = imageAddr + IMAGE_HDR_SIZE;

    if((fwAddr + pHdr->data_size) < fwAddr) {
        return false;
    }

    const uint32_t calculated = crc_calculate(fwAddr, pHdr->data_size);
    return (calculated == pHdr->crc);

}

/**
 * @brief Bir flash bölgesi üzerinde STM32 çevre birimi olan CRC32'sini hesaplar
 * @param addr Verinin başlangıç ​​adresi
 * @param size Verinin boyutu (bayt sayısı)
 * @retval Hesaplanan CRC32 değeri
 * @note Veri 4 bayt hizalı olmalıdır
 */
static uint32_t crc_calculate(uint32_t addr, uint32_t size) {

    CRC->CR = CRC_CR_RESET;

    const uint32_t *pWord = (const uint32_t *)addr;
    uint32_t words = size >> 2U;

    while (words-- > 0U) {
        CRC->DR = *pWord++;
    }

    const uint32_t remaining = size & 3U;
    if (remaining > 0U) {
        const uint8_t *pByte = (const uint8_t *)pWord;

        uint32_t last = 0U;
        for (uint32_t i = 0U; i < remaining; i++) {
            last |= (uint32_t)pByte[i] << (i * 8U);
        }
        CRC->DR = last;
    }

    return CRC->DR;
}

/**
 * @brief  Önyükleyici tarafından kullanılan çevre birimlerini devretme öncesinde devre dışı bırakır.
 * @retval None
 */
static void deinit_system(void) {
    // Kullanılan CRC çevre birimini sıfırla
    RCC->AHB1RSTR |= RCC_AHB1RSTR_CRCRST;
    RCC->AHB1RSTR &= ~RCC_AHB1RSTR_CRCRST;

    // Kullanılan APB çevre birimlerini sıfırla
    RCC->APB1RSTR |= RCC_APB1RSTR_PWRRST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_PWRRST;

    RCC->APB2RSTR |= RCC_APB2RSTR_SYSCFGRST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SYSCFGRST;

    // Çevre birimlerinin saatlerini kapat
    RCC->AHB1ENR &= ~RCC_AHB1ENR_CRCEN;
    RCC->APB1ENR &= ~RCC_APB1ENR_PWREN;
    RCC->APB2ENR &= ~RCC_APB2ENR_SYSCFGEN;

    // Tüm NVIC'leri ve bekleme bitlerini temizle
    for (uint8_t i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    // HSION bitini setle
    SET_BIT(RCC->CR, RCC_CR_HSION | RCC_CR_HSITRIM_4); 

    // CFGR kaydını sıfırla
    CLEAR_REG(RCC->CFGR);

    // HSEON, CSSON, PLLON, PLLI2S'yi sıfırla
    CLEAR_BIT(RCC->CR, RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON| RCC_CR_PLLI2SON); 

    // PLLCFGR kaydını sıfırla 
    CLEAR_REG(RCC->PLLCFGR);
    SET_BIT(RCC->PLLCFGR, RCC_PLLCFGR_PLLM_4 | RCC_PLLCFGR_PLLN_6 | RCC_PLLCFGR_PLLN_7 | RCC_PLLCFGR_PLLQ_2); 
    
    // PLLI2SCFGR kaydını sıfırla 
    CLEAR_REG(RCC->PLLI2SCFGR);
    SET_BIT(RCC->PLLI2SCFGR,  RCC_PLLI2SCFGR_PLLI2SN_6 | RCC_PLLI2SCFGR_PLLI2SN_7 | RCC_PLLI2SCFGR_PLLI2SR_1);
    
    // HSEBYP bitini sıfırla
    CLEAR_BIT(RCC->CR, RCC_CR_HSEBYP);
    
    // Tüm kesmeleri devre dışı bırak
    CLEAR_REG(RCC->CIR); 

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
}

/**
 * @brief  Vektör tablosu `vectorAddr` adresinde başlayan imajına kontrolü devreder
 * @param  vectorAddr Hedef imajın vektör tablosunun adresi
 * @retval None
 * @note Hiçbir zaman geri dönmez
 */
static __attribute__((noreturn)) void boot_to_image(uint32_t vectorAddr) {

    const uint32_t appMsp = *(const uint32_t *)(vectorAddr);
    const uint32_t appResetHandler = *(const uint32_t *)(vectorAddr + 4);

    // PendSV temizle
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
    
    // Hata işleyicilerini devre dışı bırak
    SCB->SHCSR &= ~( SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk \ 
                    | SCB_SHCSR_MEMFAULTENA_Msk ) ;

    SCB->VTOR = (uint32_t)vectorAddr;

    __set_MSP(appMsp);
	__set_PSP(appMsp);

    void (*pReset)(void) = (void (*)(void))appResetHandler;
    pReset();

    while (1) {
        __NOP();
    }
}

/* --------------------------------------------------------------------------
 * Hata / arıza işleyicileri
 * -------------------------------------------------------------------------*/

/**
 * @brief HardFault işleyicisi
 * @note Volatile okuma işlemleri, derleyicinin register değerlerini optimize ederek kaldırmasını engeller; 
 * böylece bu değerler debugger üzerinde görülebilir hale gelir
 */
void HardFault_Handler(void) {

    volatile uint32_t cfsr = SCB->CFSR;     // Yapılandırılabilir Hata Durumu
    volatile uint32_t hfsr = SCB->HFSR;     // HardFault durumu
    volatile uint32_t bfar = SCB->BFAR;     // Bus Fault adresi
    volatile uint32_t mmar = SCB->MMFAR;    // MemManage Fault adresi
    (void)cfsr; (void)hfsr; (void)bfar; (void)mmar;
    while (1) {
        __NOP();
    }
}