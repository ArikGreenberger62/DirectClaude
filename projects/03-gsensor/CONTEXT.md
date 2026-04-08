# Project 03 — gsensor — Context Map

## Current State

**Status:** Two bugs fixed and verified build-clean (0 errors, 0 warnings). Ready to flash.

Peripherals initialised: SPI1 (7.5 MHz, Mode 3), UART7 (115200 baud trace, PE7/PE8 AF7), GPIO (LEDs, P3V3_SW_EN, ACC NSS).

## Architecture Decisions

- **SPI1 clock 7.5 MHz (prescaler 16):** IOC had 15 MHz (prescaler 8) but LSM6DSO max is 10 MHz. Reduced to stay in spec. NVM shares the same bus and also tolerates 7.5 MHz.
- **Software NSS (PE9) instead of hardware NSS:** SPI bus is shared with NVM (separate NSS). Each device gets its own GPIO CS.
- **GPIO LEDs not TIM3 PWM:** Calibration needs variable blink rates (fast/slow). GPIO toggle in main loop with HAL_GetTick() is simpler than reconfiguring TIM3 capture-compare at runtime.
- **Float in calibration.c only:** Cortex-M33 FPU makes single-precision float fast. Float is isolated to calibration.c; main.c and lsm6dso.c are float-free.
- **1024 = 1G scale:** LSM6DSO at ±8G: 1G = 4096 raw counts. Output = raw >> 2 (÷4).
- **Two-position Gram-Schmidt calibration:** position 1 = gravity direction → world Z; cross product with position 2 gives world Y; X is derived. Determinant and dot-product checks validate the result.

## Last Session

- **What we just finished:** Fixed two bugs and moved trace to UART7. Build clean: 26 KB flash / 2.3 KB BSS.
- **Bug 1 fixed:** `stm32h5xx_hal_msp.c` was referencing `USART1_TX_PIN`/`USART1_RX_PIN` (never defined) → replaced entire UART MSP with UART7 (PE7/PE8, AF7, `__HAL_RCC_UART7_CLK_ENABLE`).
- **Bug 2 fixed:** `CAL_STATE_WAIT_MOVE` compared gravity *magnitude* (stays ~1G on rotation, useless) → replaced with dot-product direction check: `dot < MOVE_COS_THRESH * g1_sq` (triggers when θ > 26°).
- **Next step:** Flash to board with ST-LINK, open serial terminal at 115200 on **UART7 (PE7=RX, PE8=TX)**, verify WHO_AM_I banner, then perform calibration sequence.

## Build Issues Resolved

1. `abs()` in `calibration.c` needed `<stdlib.h>`.
2. `snprintf` buffer of 128 was too small for multiline format — split into separate calls.
3. `sqrtf` → `__errno` linker error with nano.specs: added `__errno` stub in `stm32h5xx_it.c`.

## Key Pins

| Signal        | Pin  | AF/Mode          |
|---|---|---|
| SPI1 SCK      | PA5  | AF5              |
| SPI1 MISO     | PA6  | AF5              |
| SPI1 MOSI     | PA7  | AF5              |
| ACC NSS       | PE9  | GPIO Output HIGH |
| LED_R         | PC8  | GPIO Output      |
| LED_G         | PC9  | GPIO Output      |
| P3V3_SW_EN    | PC11 | GPIO Output HIGH |
| UART7 TX      | PE8  | AF7              |
| UART7 RX      | PE7  | AF7              |
