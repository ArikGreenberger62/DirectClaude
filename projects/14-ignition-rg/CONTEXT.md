# 14-ignition-rg — CONTEXT

## Current State
Complete and verified. Build clean, flashed, trace confirmed on 2026-05-04.

## Architecture Decisions
- Derives directly from 12-ignition-status — same ADC DMA approach, same UART7 trace pattern.
- Both LED_R (PC8) and LED_G (PC9) are TIM3 CH3/CH4 in CubeMX but reconfigured as plain GPIO outputs in LED_GPIO_Init() since MX_TIM3_Init is not called. Initialised together in one HAL_GPIO_Init call for efficiency.
- P3V3_SW_EN (PC11) is already driven HIGH by MX_GPIO_Init (PinState=GPIO_PIN_SET in IOC); reinforced explicitly before ADC start for clarity.
- Threshold is strictly > 300 (not >=) per user specification "above 300".

## Last Session (2026-05-04)
- Created project from scratch based on 12-ignition-status
- Added LED_R control alongside LED_G
- Build: 0 errors, 0 warnings
- Flash: success via SWD
- Trace: raw ~1611, LED_G=ON, LED_R=OFF confirmed (ignition voltage present on PC0)
- Next step: test with ignition removed to confirm LED_R ON / LED_G OFF
