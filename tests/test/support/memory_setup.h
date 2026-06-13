#ifndef MEMORY_SETUP_H
#define MEMORY_SETUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f407xx.h"

void virtual_mem_set_all(void);

void virtual_mem_set_tim1(void);
void virtual_mem_set_tim2(void);
void virtual_mem_set_tim3(void);
void virtual_mem_set_tim4(void);
void virtual_mem_set_tim5(void);
void virtual_mem_set_tim6(void);
void virtual_mem_set_tim7(void);
void virtual_mem_set_tim8(void);
void virtual_mem_set_tim9(void);
void virtual_mem_set_tim10(void);
void virtual_mem_set_tim11(void);
void virtual_mem_set_tim12(void);
void virtual_mem_set_tim13(void);
void virtual_mem_set_tim14(void);

void virtual_mem_set_rtc(void);
void virtual_mem_set_wwdg(void);
void virtual_mem_set_iwdg(void);

void virtual_mem_set_spi1(void);
void virtual_mem_set_spi2(void);
void virtual_mem_set_spi3(void);
void virtual_mem_set_i2s2ext(void);
void virtual_mem_set_i2s3ext(void);

void virtual_mem_set_usart1(void);
void virtual_mem_set_usart2(void);
void virtual_mem_set_usart3(void);
void virtual_mem_set_usart6(void);
void virtual_mem_set_uart4(void);
void virtual_mem_set_uart5(void);

void virtual_mem_set_i2c1(void);
void virtual_mem_set_i2c2(void);
void virtual_mem_set_i2c3(void);

void virtual_mem_set_can1(void);
void virtual_mem_set_can2(void);

void virtual_mem_set_pwr(void);
void virtual_mem_set_dac(void);

void virtual_mem_set_adc1(void);
void virtual_mem_set_adc2(void);
void virtual_mem_set_adc3(void);
void virtual_mem_set_adc123_common(void);

void virtual_mem_set_sdio(void);

void virtual_mem_set_syscfg(void);
void virtual_mem_set_exti(void);

void virtual_mem_set_gpioa(void);
void virtual_mem_set_gpiob(void);
void virtual_mem_set_gpioc(void);
void virtual_mem_set_gpiod(void);
void virtual_mem_set_gpioe(void);
void virtual_mem_set_gpiof(void);
void virtual_mem_set_gpiog(void);
void virtual_mem_set_gpioh(void);
void virtual_mem_set_gpioi(void);

void virtual_mem_set_crc(void);
void virtual_mem_set_rcc(void);
void virtual_mem_set_flash(void);

void virtual_mem_set_dma1(void);
void virtual_mem_set_dma2(void);

void virtual_mem_set_dma1_stream0(void);
void virtual_mem_set_dma1_stream1(void);
void virtual_mem_set_dma1_stream2(void);
void virtual_mem_set_dma1_stream3(void);
void virtual_mem_set_dma1_stream4(void);
void virtual_mem_set_dma1_stream5(void);
void virtual_mem_set_dma1_stream6(void);
void virtual_mem_set_dma1_stream7(void);

void virtual_mem_set_dma2_stream0(void);
void virtual_mem_set_dma2_stream1(void);
void virtual_mem_set_dma2_stream2(void);
void virtual_mem_set_dma2_stream3(void);
void virtual_mem_set_dma2_stream4(void);
void virtual_mem_set_dma2_stream5(void);
void virtual_mem_set_dma2_stream6(void);
void virtual_mem_set_dma2_stream7(void);

void virtual_mem_set_eth(void);
void virtual_mem_set_dcmi(void);
void virtual_mem_set_rng(void);

void virtual_mem_set_fsmc_bank1(void);
void virtual_mem_set_fsmc_bank1e(void);
void virtual_mem_set_fsmc_bank2_3(void);
void virtual_mem_set_fsmc_bank4(void);

void virtual_mem_set_dbgmcu(void);

void virtual_mem_set_usb_otg_fs(void);
void virtual_mem_set_usb_otg_hs(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_SETUP_H */