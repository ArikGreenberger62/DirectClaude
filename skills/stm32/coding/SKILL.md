---
name: stm32/coding
description: STM32 HAL coding patterns, known bugs, and mandatory rules. Read when writing STM32 firmware code.
task_types: [code]
keywords: [stm32, hal, h573, h5xx, spi, uart, usart, tim, pwm, gpio, dma, msp, cpol, cpha, afcntr, prescaler, mode3, hal_spi_init, hal_uart, errno, nano, sqrtf, math, snprintf]
priority: tier2
---

# STM32 Coding Patterns

## Standard Include
```c
#include "stm32h5xx_hal.h"   // pulls in all HAL modules
```

## HAL Paths (this machine)
```
C:\Users\arikg\STM32Cube\Repository\STM32Cube_FW_H5_V1.6.0\
  Drivers\STM32H5xx_HAL_Driver\Inc\
  Drivers\CMSIS\
```

## UART — Interrupt-Driven RX Pattern
```c
// Re-arm in callback:
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    HAL_UART_Receive_IT(&huart7, &rx_byte, 1);  // re-arm immediately
    // set a flag; transmit from main loop — NOT from callback
}
```
- UART7 on PE7/PE8 uses `GPIO_AF7_UART7` (not AF11)

## SPI — Direct-Register Transfer (mandatory on STM32H5 V1.6.0)
`HAL_SPI_TransmitReceive` FIFO bug: reads 4 bytes even for 2-byte transfers → orphaned FIFO data.
Use direct-register pattern: manage TSIZE, SPE, CSTART manually; poll TXP/RXP per byte; wait EOT; flush FIFO; clear all flags via IFCR.
See `projects/04-gsensor/Core/Src/lsm6dso.c → spi1_xfer()` as reference.

## SPI Mode 3 — AFCNTR (M2)
```c
hspi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
```
Without this: SCK floats LOW when SPE=0 → spurious edge → 1-bit data shift.

## SPI2 Prescaler Bug (STM32H5 HAL V1.6.0)
`HAL_SPI_Init` writes CFG1 correctly but MBR bits read back as 0. Workaround:
```c
CLEAR_BIT(SPI2->CR1, SPI_CR1_SPE);
MODIFY_REG(SPI2->CFG1, SPI_CFG1_MBR, SPI_BAUDRATEPRESCALER_N);
```
Always do a register dump after init to verify.

## HAL / CubeMX Gotchas
- `FLASH_LATENCY_5` needs `HAL_FLASH_MODULE_ENABLED` in conf even without flash API
- `__HAL_RCC_SYSCFG_CLK_ENABLE` / `__HAL_RCC_PWR_CLK_ENABLE` do NOT exist on STM32H5 — remove from `HAL_MspInit`
- Do NOT call `MX_ICACHE_Init` unless confirmed necessary
- TIM PWM: `HAL_TIM_PWM_Init` only — `HAL_TIM_Base_Init` on same handle causes MspInit to be skipped

## Float Math with nano.specs (M3)
```cmake
target_link_libraries(<PROJECT_NAME> PRIVATE -lm)
```
```c
/* in stm32h5xx_it.c */
int *__errno(void) { static int e = 0; return &e; }
```
Both required together.

## snprintf Buffer
Keep buffers large enough for worst-case format string or split messages.
`-Werror=format-truncation` will catch statically-provable overflows.
