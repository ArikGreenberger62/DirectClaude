/* stm32h5xx_hal_conf.h — HAL module enable for 02-uart7-echo project */
#ifndef STM32H5XX_HAL_CONF_H
#define STM32H5XX_HAL_CONF_H

/* ── Enabled HAL modules ─────────────────────────────────────────────────── */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED      /* required for FLASH_LATENCY_5 in RCC */
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ── HSE / HSI values ────────────────────────────────────────────────────── */
#if !defined(HSE_VALUE)
  #define HSE_VALUE  12000000UL   /* 12 MHz crystal on TFL_CONNECT_2 board */
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

/* ── VDD ─────────────────────────────────────────────────────────────────── */
#if !defined(VDD_VALUE)
  #define VDD_VALUE  3300UL   /* mV */
#endif

/* ── Tick frequency ──────────────────────────────────────────────────────── */
#define TICK_INT_PRIORITY  0x0FUL

/* ── SysTick ─────────────────────────────────────────────────────────────── */
#define USE_RTOS   0U
#define USE_SYSTICK_AS_OS_TICK  0U
#define USE_HAL_ASSERT  1U
#define USE_SPI_CRC  1U

/* ── Assert macro ────────────────────────────────────────────────────────── */
#define assert_param(expr) ((void)0U)

/* ── Include HAL drivers ─────────────────────────────────────────────────── */
#include "stm32h5xx_hal_rcc.h"
#include "stm32h5xx_hal_rcc_ex.h"
#include "stm32h5xx_hal_gpio.h"
#include "stm32h5xx_hal_cortex.h"
#include "stm32h5xx_hal_pwr.h"
#include "stm32h5xx_hal_pwr_ex.h"
#include "stm32h5xx_hal_uart.h"
#include "stm32h5xx_hal_uart_ex.h"
#include "stm32h5xx_hal_flash.h"
#include "stm32h5xx_hal_flash_ex.h"

#endif /* STM32H5XX_HAL_CONF_H */
