# 12-ignition-status — STATE (compact session cache)

## Status
Build clean (0 errors, 0 warnings). Flashed and verified 2026-05-03.
Trace confirmed: ADC calibration PASS, DMA PASS, raw ~1612 → LED_G ON.

## Key facts
- ADC Ignition channel: PC0 / ADC1_INP10 / CH10 / Rank 3 / g_adc_buf[2]
- Threshold: raw >= 300 → LED_G ON; < 300 → OFF (checked every 10 ms in main loop)
- LED_G (PC9) is TIM3 CH4 in CubeMX; MX_TIM3_Init() NOT called — LED_G_Init()
  configures it as GPIO_MODE_OUTPUT_PP after MX_GPIO_Init().
- LED_G is ACTIVE-LOW: GPIO_PIN_RESET = LED ON, GPIO_PIN_SET = LED OFF.
- P3V3_SW_EN (PC11) set HIGH by LowLevel gpio.c (initial HIGH output); re-asserted
  in main() for clarity. Permanently ON.
- UART7 trace every 2 s on COM7 @ 115200.
- HAL timebase: TIM4 (not SysTick).

## Bug fixes locked in
- LED_G active-low polarity: RESET=ON, SET=OFF. Do NOT revert to SET=ON.

## Next step
No outstanding work. Project done.
