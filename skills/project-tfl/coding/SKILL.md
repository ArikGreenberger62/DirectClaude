---
name: project-tfl/coding
description: TFL_CONNECT_2 board-specific coding rules and lessons learned. Read when writing code for this workspace.
---

# TFL_CONNECT_2 — Coding Rules & Lessons

## Mandatory Rules (apply every project)

**M2 — SPI Mode 3:** `MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE` always.
**M3 — Float math:** `-lm` + `__errno` stub always when using `<math.h>`.
**M4 — SPI polling:** Never use `HAL_SPI_TransmitReceive` — use direct-register. See `projects/04-gsensor/Core/Src/lsm6dso.c → spi1_xfer()`.
**M7 — TIM PWM:** `HAL_TIM_PWM_Init` only. Never pair with `HAL_TIM_Base_Init`.
**M8 — Trace UART:** Every project must have `Trace_Print()` on UART7, 115200 8N1.

## Trace Function (required)
```c
void Trace_Print(const char *msg, uint16_t len) {
    HAL_UART_Transmit(&huart7, (uint8_t *)msg, len, HAL_MAX_DELAY);
}
```
- Every init step: `[MODULE] description: PASS` / `FAIL - reason`
- Print register values after peripheral init (catches SPI prescaler bugs)
- 1 Hz runtime: tag prefix (`RAW`, `CAL`, `MOVE`, etc.)

## Lessons Learned

### HAL / CubeMX
- `FLASH_LATENCY_5` → enable `HAL_FLASH_MODULE_ENABLED` in conf
- `__HAL_RCC_SYSCFG_CLK_ENABLE` / `__HAL_RCC_PWR_CLK_ENABLE` don't exist on STM32H5 → remove from `HAL_MspInit`
- Do not call `MX_ICACHE_Init` unless required

### SPI
- SPI Mode 3 + AFCNTR=0 → spurious clock edge → 1-bit data shift (see M2)
- STM32H5 HAL V1.6.0 `HAL_SPI_TransmitReceive` FIFO bug (see M4)
- STM32H5 HAL V1.6.0 SPI2 prescaler not applied → force via direct register after init; verify with dump

### UART
- UART7 PE7/PE8 → `GPIO_AF7_UART7` (not AF11)
- Interrupt RX: re-arm in `HAL_UART_RxCpltCallback`; never call blocking TX from callback

### Build / Tooling
- Capture startup trace: open serial port FIRST, then trigger hard reset via programmer CLI
- `HAL_SPI_Init` may silently fail → always dump registers after init
- `snprintf` + `-Werror=format-truncation`: keep buffers large enough for worst-case format
- `__builtin_sqrtf` at `-O0` does NOT inline → needs `-lm`
