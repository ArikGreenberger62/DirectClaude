# 14-ignition-rg — STATE (compact session cache)

## Status
Build clean (0 errors, 0 warnings). Flashed and trace verified 2026-05-04.

## Key facts
- ADC IgnitionPCO = PC0 / ADC1_INP10 / Rank 3 / DMA buffer index [2]
- Threshold: raw > 300 → LED_G ON + LED_R OFF; else LED_R ON + LED_G OFF
- LEDs active-low: GPIO_PIN_RESET = LED ON, GPIO_PIN_SET = LED OFF
- Both LED_R (PC8) and LED_G (PC9) are TIM3 CH3/CH4 in CubeMX — MX_TIM3_Init NOT called; both manually init'd as GPIO outputs in LED_GPIO_Init()
- P3V3_SW_EN (PC11) set HIGH by MX_GPIO_Init, reinforced before ADC start
- Trace: UART7 (PE7/PE8, AF7, 115200 8N1), every 2 s
- Verified trace: raw ~1611, LED_G=ON, LED_R=OFF (ignition line live)

## Bug fixes locked in
- MX_TIM3_Init must NOT be called — calling it would overwrite PC8/PC9 AF and break GPIO control
- GPDMA1_Init must precede ADC1_Init (DMA channel 0 links to ADC1 in MspInit)

## Next step
Ready to flash test with ignition removed to confirm LED_R ON / LED_G OFF transition.
