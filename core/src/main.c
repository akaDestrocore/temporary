/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "image.h"
#include "nextion.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    APP_STATE_POST_LOAD = 0U,
    APP_STATE_MENU      = 1U
} AppState_e;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim9;
TIM_HandleTypeDef htim13;
TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

static Nextion_Config_t nextion;

static AppState_e gAppState = APP_STATE_POST_LOAD;
static uint32_t gLastToggleTick = 0U;
static bool gLedState = false;
#define UPDATER_VECTOR_ADDR (UPDATER_ADDR + IMAGE_HDR_SIZE)

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void USART3_IRQHandler(void);
/* USER CODE BEGIN PFP */
static void deinit_system(void);
static void jumpToUpdater(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration -------------------------------------------------------*/
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    MX_GPIO_Init();

    /* USER CODE BEGIN 2 */
    // -- Nextion --------------------------------------------------
    nextion.pUsart = USART3;
    nextion.pGpio_port = GPIOB;
    nextion.rx_pin_num = 11U;
    nextion.tx_pin_num = 10U;
    nextion.gpio_af = 7U;
    nextion.gpio_ahb1_bits = RCC_AHB1ENR_GPIOBEN;
    nextion.apbx_bit = RCC_APB1ENR_USART3EN;
    nextion.is_apb2 = 0U;
    nextion.apb_freq_hz = HAL_RCC_GetPCLK1Freq();
    nextion.baud_rate = 9600U;
    nextion.irqn = USART3_IRQn;
    nextion.nvic_priority = 8U;
    nextion.ack_timeout_ms = 200U;
    (void)nextion_init(&nextion);

    (void)nextion_sendCmd("n0.val=0");
    (void)nextion_sendCmd("tm0.en=0");
    (void)nextion_sendCmd("tm1.en=0");
    (void)nextion_sendCmd("j0.val=100");
    (void)nextion_sendCmd("page 35");
    char cmdBuff[NEXTION_CMD_MAX_LEN + 1U];
    (void)snprintf(cmdBuff, sizeof(cmdBuff), "page35.t1.txt=\"VERSION:V%02u.%02u.%02u\"", (unsigned int)image_header.version_major,
                                    (unsigned int)image_header.version_minor,(unsigned int)image_header.version_patch);
    (void)nextion_sendCmd(cmdBuff);
    HAL_Delay(1000);
    memset(cmdBuff, 0x00, sizeof(cmdBuff));
    (void)snprintf(cmdBuff, sizeof(cmdBuff), "page35.t2.txt=\"BUILD:%s\"",image_header.git_sha);
    (void)nextion_sendCmd(cmdBuff);

    gAppState = APP_STATE_POST_LOAD;
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
    /* USER CODE END WHILE */
    
    /* USER CODE BEGIN 3 */
        nextion_process();

        uint32_t now = HAL_GetTick();
        if ((now - gLastToggleTick) >=500) {
            gLastToggleTick = now;
            gLedState = !gLedState;
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, gLedState ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        Nextion_Frame_t frame;

        switch(gAppState) {
            case APP_STATE_POST_LOAD: {
                while(NEXTION_STATUS_OK == nextion_readFrame(&frame)) {
                    if ((NEXTION_FRAME_TOUCH == frame.type) &&
                        (0x65 == frame.code) && (3U <= frame.length) &&
                        (0x01U == frame.data[0]) && (0x09U == frame.data[1]) &&
                        (0x01U == frame.data[2])) {
                            (void)nextion_sendCmd("page 20");

                            uint32_t deadline = HAL_GetTick() + 1000U;
                            while (true == nextion_isBusy()) {
                                nextion_process();
                                if (HAL_GetTick() > deadline) {
                                    break;
                                }
                            }

                            gAppState = APP_STATE_MENU;
                            break;
                    }
                }
                break;
            }

            case APP_STATE_MENU: {
                while (NEXTION_STATUS_OK == nextion_readFrame(&frame)) {
                    if ((NEXTION_FRAME_TOUCH == frame.type) &&
                        (0x65 == frame.code) && (3U <= frame.length) &&
                        (0x24U == frame.data[0]) && (0x08U == frame.data[1]) &&
                        (0x01U == frame.data[2])) {
                            
                            (void)nextion_sendCmd("page 34");

                            uint32_t deadline = HAL_GetTick() + 1000U;
                            while (true == nextion_isBusy()) {
                                nextion_process();
                                if (HAL_GetTick() > deadline) {
                                    break;
                                }
                            }
                        
                            if (NEXTION_STATUS_OK != nextion_getLastCommandStatus()) {
                                break; // ACK gelmedi
                            }

                            deinit_system();
                            jumpToUpdater();
                    }
                }
                break;

            }

            default: {
                gAppState = APP_STATE_POST_LOAD;
                break;
            }

        }
    }
    /* USER CODE END 3 */
}

static void SystemClock_Config(void)
{

    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8U;
    RCC_OscInitStruct.PLL.PLLN = 336U;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7U;

    if (HAL_OK != HAL_RCC_OscConfig(&RCC_OscInitStruct))
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

    if (HAL_OK != HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5))
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{

    GPIO_InitTypeDef gpioInitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PD12 — LED
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);

    gpioInitStruct.Pin = GPIO_PIN_15;
    gpioInitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInitStruct.Pull = GPIO_NOPULL;
    gpioInitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpioInitStruct);

    // PA0 — user button
    gpioInitStruct.Pin = GPIO_PIN_0;
    gpioInitStruct.Mode = GPIO_MODE_INPUT;
    gpioInitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpioInitStruct);
}

/* USER CODE BEGIN 4 */
static void deinit_system(void) {

    FLASH->ACR &= ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);
    FLASH->ACR |=  (FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    FLASH->ACR &= ~(FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    __DSB();

    GPIOD->BSRR  = (GPIO_BSRR_BS12 | GPIO_BSRR_BS13
                  | GPIO_BSRR_BS14 | GPIO_BSRR_BS15) << 16U;
    GPIOD->MODER  = 0x00000000U;
    GPIOD->OTYPER = 0x00000000U;
    GPIOD->PUPDR  = 0x00000000U;

    RCC->AHB1RSTR |=  (RCC_AHB1RSTR_CRCRST | RCC_AHB1RSTR_GPIOARST
                     | RCC_AHB1RSTR_GPIODRST);
    RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_CRCRST | RCC_AHB1RSTR_GPIOARST
                     | RCC_AHB1RSTR_GPIODRST);

    RCC->APB1RSTR |=  RCC_APB1RSTR_PWRRST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_PWRRST;

    RCC->APB2RSTR |=  RCC_APB2RSTR_SYSCFGRST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SYSCFGRST;

    RCC->AHB1ENR &= ~(RCC_AHB1ENR_CRCEN  | RCC_AHB1ENR_GPIOAEN
                    | RCC_AHB1ENR_GPIODEN);
    RCC->APB1ENR &= ~RCC_APB1ENR_PWREN;
    RCC->APB2ENR &= ~RCC_APB2ENR_SYSCFGEN;

    for (uint32_t i = 0U; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    __DSB();
    __ISB();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    SCB->ICSR    |= SCB_ICSR_PENDSTCLR_Msk;
}

static void jumpToUpdater(void) {
    
    // LED kapatma
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);

    // Kontrol aktarımı sırasında tetiklenmemesi için SysTick'i devre dışı bırakma
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    // Tüm olası IRQ'lar için NVIC etkinleştirme ve bekleyen bayrakların temizlenmesi
    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    // gpio_init() tarafından etkinleştirilen çevre birimi saatlerinin kapatılması
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOD_CLK_DISABLE();

    // Stack pointer'a dokunmadan önce tüm kesmeleri global olarak devre dışı bırakma
    __disable_irq();

    // Vector table updater'ın ISR vektör bölümüne taşıma
    SCB->VTOR = UPDATER_VECTOR_ADDR;

    // Updater'ın vektör tablosundan başlangıç stack pointer'ını ve
    // reset işleyicisini oku, ardından yürütmeyi ona devret
    uint32_t updaterMsp = *((volatile uint32_t *)(UPDATER_VECTOR_ADDR));
    uint32_t updaterResetFn = *((volatile uint32_t *)(UPDATER_VECTOR_ADDR + 4U));

    __set_MSP(updaterMsp);

    // Fonksiyon işaretçisine dönüştür ve dallan — geri dönmez
    ((void (*)(void))updaterResetFn)();

    // Bu noktaya normalde ulaşılamaz; noreturn uyarısı veren derleyicileri susturur
    while (1)
    {
        __NOP();
    }
}

/**
 * @brief USART3 IRQ Handler - Nextion ekrani için
 * @retval None
 */
void USART3_IRQHandler(void) {
    nextion_irqHandler();
}

/* USER CODE END 4 */

/**
 * @brief  Error handler — halts execution with interrupts disabled.
 * @retval None
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        __NOP();
    }
}

/**
 * @brief  HardFault handler — captures fault registers for debugger inspection.
 * @retval None
 */
void HardFault_Handler(void) {
    volatile uint32_t cfsr = SCB->CFSR;    // Configurable Fault Status
    volatile uint32_t hfsr = SCB->HFSR;   // HardFault Status
    volatile uint32_t bfar = SCB->BFAR;   // Bus Fault Address
    volatile uint32_t mmar = SCB->MMFAR;  // MemManage Fault Address
    (void)cfsr; (void)hfsr; (void)bfar; (void)mmar;

    while (1) {
        __NOP();
    }
}