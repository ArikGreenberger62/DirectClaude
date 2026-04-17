# ThreadX — blink demo

Two blinkers driven by Azure RTOS ThreadX:
- **LED_R (PC8)** — toggled every 500 ms by a `tx_timer` callback (`RedTimer_Cb` in `Core/Src/app_threadx.c`).
- **LED_G (PC9)** — toggled every 500 ms by a dedicated thread (`Green_Entry`, prio 10, 1 KB stack).

Net blink rate for both: 1 Hz, 50% duty cycle.

## Layout highlights
- No `LowLevel/` dependency — HAL is pulled directly from `STM32Cube_FW_H5_V1.6.0` (same pattern as `01-blink`). Keeps the project self-contained for learning ThreadX.
- Static byte pool (4 KB) in BSS; green thread stack is carved out of it — no linker-script changes.
- `Core/Src/tx_initialize_low_level.S` is a **local copy** of the Cortex-M33 GNU port file, with `SYSTEM_CLOCK` retuned from the port's default 250 MHz to our **240 MHz** so SysTick ticks at exactly 100 Hz (10 ms).
- `Core/Inc/tx_user.h` sets `TX_TIMER_TICKS_PER_SECOND=100U` to match.
- CMake adds `-DTX_SINGLE_MODE_NON_SECURE` so the Cortex-M33 TrustZone secure-stack glue is stripped (H573 runs non-secure).

## Build
```
cmake.exe --preset Debug
cmake.exe --build --preset Debug
```
Result: ~10 KB flash, ~7.5 KB RAM.

## Gotchas hit
1. **Don't define `TX_SINGLE_MODE_NON_SECURE` in both `tx_user.h` and CMake `-D`** — Werror redefinition. Keep it in CMake only.
2. **The port's `tx_initialize_low_level.S` hardcodes `SYSTEM_CLOCK = 250000000`.** Using it unchanged gives a ~4% slow tick. Copy it locally and retune.
3. **`PendSV_Handler` / `SVC_Handler`** are provided by `tx_thread_schedule.S` — do not write your own.
