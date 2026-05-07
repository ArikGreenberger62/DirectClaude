# 16-ignition-led — STATE (compact session cache)

## Status
Build clean (0 errors, 0 warnings). Flashed and trace-verified 2026-05-04.

## Key facts
- ADC IgnitionPCO = PC0, ADC1_INP10, Rank 3, DMA buffer index [2]
- Threshold: raw > 300 → LED_R ON (ignition present); raw ≤ 300 → LED_G ON
- LEDs active-low: GPIO_PIN_RESET = LED ON, GPIO_PIN_SET = LED OFF
- P3V3_SW_EN (PC11): set HIGH by MX_GPIO_Init, reinforced before peripheral init
- MX_TIM3_Init NOT called — PC8/PC9 used as plain GPIO push-pull, not PWM AF
- GPDMA1_Init must precede ADC1_Init
- HAL timebase: TIM4 (SysTick_Handler is empty stub)
- UART7 trace: COM7 @ 115200, every 2 s

## Bug fixes locked in
- Do not call MX_TIM3_Init — it would override PC8/PC9 GPIO config with TIM3 AF

## Verified trace (2026-05-04, ignition line live ~1612 counts)
```
[IGN] 16-ignition-led starting
[IGN] IgnitionPCO=PC0/ADC1_INP10 threshold=300 | LED_R=PC8 | LED_G=PC9 | P3V3_SW_EN=PC11 HIGH
[IGN] ADC calibration: PASS
[IGN] ADC DMA started: PASS
[IGN] raw=1612  LED_R=ON  LED_G=OFF
```

## Next step
Flash and verify LED_G behaviour by disconnecting ignition (raw should drop below 300).
