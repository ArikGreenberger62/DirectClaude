# ThreadX — STATE (compact session cache)

> Auto-load file. Full archive: `CONTEXT.md`.
> Skill reference: `skills/stm32/threadx/SKILL.md`.

## Status
DONE. ~10 KB flash / ~7.5 KB RAM. Two blinkers running on Azure RTOS.

## Key facts
- LED_R PC8 — toggled every 500 ms by a `tx_timer` callback.
- LED_G PC9 — toggled every 500 ms by a dedicated thread (prio 10, 1 KB stack).
- Static 4 KB byte pool in BSS (no linker-script changes).
- HAL pulled directly from `STM32Cube_FW_H5_V1.6.0` — no `LowLevel/` dependency.
- `Core/Src/tx_initialize_low_level.S` is a **local copy**, `SYSTEM_CLOCK`
  retuned to **240 MHz** so SysTick = 100 Hz exactly.
- `Core/Inc/tx_user.h` sets `TX_TIMER_TICKS_PER_SECOND=100U` to match.
- CMake flag: `-DTX_SINGLE_MODE_NON_SECURE` (H573 non-secure; defined ONCE).

## Gotchas locked in
1. Don't define `TX_SINGLE_MODE_NON_SECURE` in both `tx_user.h` and CMake (Werror redef).
2. Port file's `SYSTEM_CLOCK = 250000000` default → ~4 % slow tick if unmodified.
3. `PendSV_Handler` / `SVC_Handler` come from the port — do NOT write your own.

## Next step
Frozen. Use as the canonical reference for future ThreadX-based projects.
