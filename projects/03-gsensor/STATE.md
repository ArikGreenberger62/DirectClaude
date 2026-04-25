# 03-gsensor — STATE (compact session cache)

> Auto-load file. Compact summary of where this project stands.
> Full archive: `CONTEXT.md` + `CLAUDE.md` (load only if STATE is insufficient).

## Status
DONE. Build clean (26 KB flash / 2.3 KB BSS). USART1 trace.

## Key facts
- LSM6DSO over SPI1 7.5 MHz, Mode 3, software NSS PE9.
- Two-position Gram-Schmidt calibration (in `calibration.c`, float-isolated).
- LEDs PC8/PC9 toggled in main loop via `HAL_GetTick()` (no TIM3 PWM).
- Trace UART = USART1 PA9 (note: project 04 uses UART7).

## Bug fixes locked in
- `stm32h5xx_hal_msp.c` UART init rewritten for UART7 PE7/PE8 AF7.
- `CAL_STATE_WAIT_MOVE` uses dot-product direction (not magnitude).
- `__errno` stub added in `stm32h5xx_it.c` (nano.specs + `sqrtf`).

## Next step
Re-flash + verify if any change is requested. Otherwise frozen.

## See also
- `c-embedded/devices/lsm6dso.md` — sensor knowledge file.
- `projects/04-gsensor/` — successor project (UART7, AFCNTR fix).
