/* stm32h5xx_hal_conf.h — HAL module enable for ThreadX blink project */
#ifndef STM32H5XX_HAL_CONF_H
#define STM32H5XX_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED

#if !defined(HSE_VALUE)
  #define HSE_VALUE  12000000UL
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT  100UL
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE  64000000UL
#endif
#if !defined(HSI48_VALUE)
  #define HSI48_VALUE  48000000UL
#endif
#if !defined(CSI_VALUE)
  #define CSI_VALUE  4000000UL
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE  32768UL
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT  5000UL
#endif
#if !defined(LSI_VALUE)
  #define LSI_VALUE  32000UL
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE  12288000UL
#endif

#if !defined(VDD_VALUE)
  #define VDD_VALUE  3300UL
#endif

#define TICK_INT_PRIORITY  0x0FUL
#define USE_RTOS   0U
#define USE_SYSTICK_AS_OS_TICK  0U
#define USE_HAL_ASSERT  0U

#define assert_param(expr) ((void)0U)

#include "stm32h5xx_hal_rcc.h"
#include "stm32h5xx_hal_rcc_ex.h"
#include "stm32h5xx_hal_gpio.h"
#include "stm32h5xx_hal_cortex.h"
#include "stm32h5xx_hal_pwr.h"
#include "stm32h5xx_hal_pwr_ex.h"
#include "stm32h5xx_hal_flash.h"
#include "stm32h5xx_hal_flash_ex.h"
#include "stm32h5xx_hal_dma.h"
#include "stm32h5xx_hal_dma_ex.h"
#include "stm32h5xx_hal_uart.h"
#include "stm32h5xx_hal_uart_ex.h"

#endif /* STM32H5XX_HAL_CONF_H */
