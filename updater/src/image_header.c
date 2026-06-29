/**
 * @file   image_header.c
 * @brief  Firmwarte'in imaj başlığını uygulama flash bölgesinin başlangıcına
 *         yerleştirir (FLASH_HDR, adres 0x08004000).
 *
 * @author  destrocore
 * @date    2026
 * 
 * crc, data_size ve vector_addr alanları başlangıçta sıfırdır ve
 * linkleme işleminden sonra scripts/patch_image_header.py tarafından
 * yerinde güncellenir.
 *
 * IMAGE_HDR_SIZE (0x200 = 512 byte) linker script tarafından zorunlu kılınır:
 * .image_hdr bölümü 0x200 boyutuna hizalanır/doldurulur.
 */

#include "image.h"

/* GIT_COMMIT_HASH, CMake tarafından derleme tanım stringi olarak eklenir.
   8 hexadecimal karakter + null sonlandırıcıdan oluşur (9 byte) ve [16]
   sınırının içerisindedir. */
const Image_Hdr_t __attribute__((section(".image_hdr"))) __attribute__((used))
image_header = {
    .image_magic       = IMAGE_MAGIC_UPDATER,
    .image_hdr_version = IMAGE_HDR_VERSION,
    .image_type        = IMAGE_TYPE_UPDATER,
    .version_major     = 1,
    .version_minor     = 0,
    .version_patch     = 0,
    ._padding          = 0,
    .vector_addr       = 0,
    .crc               = 0,
    .data_size         = 0,
    .git_sha           = GIT_COMMIT_HASH,
    /* .reserved alanı varsayılan olarak sıfır ile başlatılır */
};