## Current State
Project created 2026-05-03. All source files written, build not yet run.

## Architecture Decisions
- ADC1 DMA (9-channel circular, same as 11-adc-check): reads g_adc_buf[2] for Ignition (PC0/CH10/rank3).
- LED_G (PC9) is a TIM3 CH4 PWM pin in CubeMX but used here as plain GPIO output.
  MX_TIM3_Init() is NOT called so the AF is never applied; LED_G_Init() configures PC9
  as GPIO_MODE_OUTPUT_PP after MX_GPIO_Init().
- P3V3_SW_EN (PC11) is configured HIGH by MX_GPIO_Init() (LowLevel gpio.c) and
  explicitly re-asserted in main() for clarity.
- Threshold: raw ADC >= 300 → LED_G ON; < 300 → LED_G OFF.
- Trace via UART7 every 2 s: raw value + LED state string.

## Last Session
- What we just finished: All files created (main.h, main.c, stm32h5xx_it.c, CMakeLists.txt, CMakePresets.json).
- What is currently broken or incomplete: Build not yet run.
- What the next step is: cmake --preset Debug && cmake --build --preset Debug; then flash and verify trace.
