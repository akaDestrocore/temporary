/**
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║                                TRQ-2026                               ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * @file   app_error.h
 * @brief  Middleware tarafından kullanılmak üzere Error_Handler() ortak bildirimi.
 *
 * @details
 *  Middleware translation unit’leri (usb_host.c, usbh_conf.c)
 *  updater.h dosyasını include etmemelidir — aksi halde uygulama
 *  seviyesindeki tipler ve kart yapılandırma makroları middleware
 *  katmanına taşınmış olur.
 *
 *  Bu küçük header, middleware’in uygulama katmanından meşru olarak
 *  ihtiyaç duyduğu tek sembolü sağlar: kritik hata işleyicisi.
 *
 *  Gerçek implementasyon updater.c içinde bulunur.
 *  Error_Handler() çağıran ve updater.c olmayan her translation unit,
 *  updater.h yerine bu header dosyasını include etmelidir.
 */

#ifndef APP_ERROR_H
#define APP_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Hata işleyicisi
  * @retval None
  */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ERROR_H */