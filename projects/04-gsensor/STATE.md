# 04-gsensor — STATE (compact session cache)

> Auto-load file. Compact summary of where this project stands.
> Full archive: `CONTEXT.md` (load only if STATE is insufficient).
> **Canonical reference project — copy its layout for new projects.**

## Status
DONE. Verified on hardware (COM7, UART7).
Build: flash 79,612 B / 2 MB; RAM 6,400 B / 640 KB.

## Key facts
- LSM6DSO over SPI1 7.5 MHz, **Mode 3 + AFCNTR=1** (SPI_MASTER_KEEP_IO_STATE_ENABLE).
- `spi1_xfer()` uses direct-register transfers (avoids HAL V1.6.0 FIFO bug).
- Trace UART7 (PE7/PE8 AF7) @ 115200 — **TFL standard**.
- LEDs PC8/PC9 GPIO toggled in main loop. P3V3_SW_EN PC11 HIGH.
- HAL tick from TIM4 (`stm32h5xx_hal_timebase_tim.c`); SysTick handler empty.

## LowLevel reuse pattern (template for new projects)
Copied + edited locally:
- `Core/Src/spi.c` — SPI1 Mode 3, prescaler 16, **AFCNTR=1**.
- `Core/Src/stm32h5xx_it.c` — adds `__errno` stub.
- `Core/Src/main.c` — uses LowLevel `SystemClock_Config`; adds LED + app loop.

All other LowLevel sources referenced via `${LL}/...` in CMakeLists.

## Next step
Frozen. Use as reference when scaffolding new projects.
