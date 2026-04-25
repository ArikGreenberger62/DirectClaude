# 07-dual-blink — STATE (compact session cache)

> Auto-load file. Full archive: `CONTEXT.md`.

## Status
Build clean (67 KB flash / 6 KB RAM). **Not yet flashed** to hardware.

## Key facts
- LED_R PC8 / LED_G PC9, GPIO software state-machine via `HAL_GetTick() % 1000`.
- Pattern: RED ON 0–199 ms, GREEN ON 200–999 ms (1 Hz, complement, 20 % red duty).
- No TIM3 PWM used.
- LowLevel reuse: `usart.c` as-is; `gpio.c` overridden (only PC8/PC9/PC11);
  `stm32h5xx_it.c` overridden (adds `__errno` stub).

## Next step
1. `STM32_Programmer_CLI -l st`
2. `STM32_Programmer_CLI -c port=SWD freq=4000 -w build/Debug/dual_blink.elf -rst`
3. Open COM7 @ 115200 — confirm trace + visual LED pattern.
