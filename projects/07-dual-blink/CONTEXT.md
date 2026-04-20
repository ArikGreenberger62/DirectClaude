## Current State

Build passes cleanly (0 errors, 0 warnings). 67 KB flash, 6 KB RAM.
Not yet flashed to hardware.

Files created:
- `CMakeLists.txt`, `CMakePresets.json`, `.vscode/` — build and IDE config
- `Core/Inc/main.h` — board pin macros (copy of LowLevel) + LED aliases
- `Core/Src/main.c` — clock init + GPIO/UART init + LED state-machine loop
- `Core/Src/gpio.c` — overrides LowLevel gpio.c: configures PC8/PC9/PC11 only
- `Core/Src/stm32h5xx_it.c` — full ISR table copy + `__errno` stub

## Architecture Decisions

### LED control: software state machine via HAL_GetTick()
Using `HAL_GetTick() % 1000` as a phase counter. GREEN is the bitwise complement
of RED — computed in one line, no separate timing constants needed.

### Timing map (1000 ms cycle)
```
t =   0..199  RED = ON,  GREEN = OFF   (200 ms = 20 % duty)
t = 200..999  RED = OFF, GREEN = ON    (800 ms)
```
Both LEDs at 1 Hz. GREEN is exact inverse of RED. ✓

### LowLevel reuse
- `usart.c` used as-is (UART7 at 115200, blocking TX for trace)
- `gpio.c` overridden (LowLevel version configures all board GPIOs; this
  project only needs PC8/PC9/PC11 and avoids side-effects on other pins)
- `stm32h5xx_it.c` overridden (adds `__errno` stub; identical ISR table)

### HAL timebase
TIM4 is the HAL tick source (`stm32h5xx_hal_timebase_tim.c` from LowLevel).
SysTick_Handler is empty. `HAL_GetTick()` returns the TIM4-driven millisecond
counter. ✓

### No TIM3 used
LED pins PC8/PC9 are GPIO_Output (not TIM3 AF). `HAL_TIM_PWM_Init` is never
called, so TIM3 MSP is never triggered.

## Last Session

- **Finished:** corrected LED pattern — RED 1 Hz 20% duty, GREEN = exact complement
- **Pattern change:** previous code had RED at 30% duty with a complex 2-pulse
  GREEN pattern. New requirement: simple complement (GREEN ON = RED OFF).
- **Next step:** flash with STM32_Programmer_CLI and verify LED pattern on
  hardware via UART7 trace
