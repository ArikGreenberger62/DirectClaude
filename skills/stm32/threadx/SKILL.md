---
name: stm32/threadx
description: Azure RTOS ThreadX integration on STM32H573 (Cortex-M33 GNU port). Read when starting or modifying a ThreadX-based project in this workspace.
---

# STM32H573 + Azure RTOS ThreadX

Canonical reference: `projects/ThreadX/` (LED_R via `tx_timer`, LED_G via dedicated thread).

## Sources (STM32Cube FW_H5 V1.6.0)

| Component | Path |
|---|---|
| Common C sources (185 files) | `Middlewares/ST/threadx/common/src/*.c` — glob all |
| Cortex-M33 GNU port | `Middlewares/ST/threadx/ports/cortex_m33/gnu/src/*.S` |
| Public headers | `Middlewares/ST/threadx/common/inc/` + `.../ports/cortex_m33/gnu/inc/` |

The port provides strong `PendSV_Handler`, `SVC_Handler`, and `_tx_timer_interrupt` — do **not** define your own.

## Required CMake defines

```cmake
target_compile_definitions(${TARGET} PRIVATE
    TX_INCLUDE_USER_DEFINE_FILE    # so tx_port.h pulls in Core/Inc/tx_user.h
    TX_SINGLE_MODE_NON_SECURE      # strips TrustZone secure-stack glue on H573 non-secure
)
```

## Gotchas

### 1. SYSTEM_CLOCK in `tx_initialize_low_level.S` is hardcoded to 250 MHz
The port's SysTick reload = `(SYSTEM_CLOCK / 100) - 1`. With our 240 MHz SYSCLK, an unmodified port file ticks at ~104 Hz — all `tx_thread_sleep`/`tx_timer` periods will be ~4% off.

**Fix:** copy `tx_initialize_low_level.S` to `Core/Src/`, patch `SYSTEM_CLOCK = 240000000`, and **exclude** the port copy from your CMake `TX_PORT_SOURCES` list.

The vector-table symbol on GCC is `g_pfnVectors` (from CMSIS `startup_stm32h573xx.s`).

### 2. Don't redefine `TX_SINGLE_MODE_NON_SECURE`
Putting it both in `tx_user.h` and as a `-D` flag triggers `-Werror=redefined`. Keep it in one place (CMake is cleanest).

### 3. `tx_user.h` requires `TX_INCLUDE_USER_DEFINE_FILE`
Without that define, `tx_port.h` ignores your user file and you get ThreadX defaults (e.g. 1000 Hz tick assumed) → mismatch with SysTick reload.

### 4. Static vs dynamic byte pool
Static allocation (a BSS buffer backing `tx_byte_pool_create`) keeps the LowLevel linker script untouched — preferred. Dynamic allocation needs a `._threadx_heap` section in the .ld file and `#define USE_DYNAMIC_MEMORY_ALLOCATION` in the port asm.

### 5. HAL_Delay / HAL_GetTick after kernel enter
`tx_initialize_low_level.S` owns `SysTick_Handler` → `HAL_IncTick` never runs, so `HAL_GetTick` freezes. Either:
- use `tx_thread_sleep(ticks)` from threads, or
- install `stm32h5xx_hal_timebase_tim.c` (LowLevel provides one using TIM4) to drive HAL tick from a TIM.

Pure ThreadX demos (like `projects/ThreadX/`) just avoid HAL_Delay.

## Timer vs thread — when to pick what

| Job | Use |
|---|---|
| Short, periodic, no blocking calls (toggle a pin, sample a register, set a flag) | `tx_timer` — runs in the timer-thread context, no stack of its own |
| Anything that sleeps, waits on a queue/semaphore, does long work | `tx_thread` + `tx_thread_sleep` |

`tx_timer` callbacks are **not** ISRs — `HAL_GPIO_TogglePin` is fine. But they share one stack with every other timer, so keep them tiny.

## Boilerplate

`main.c` → `HAL_Init(); SystemClock_Config(); MX_GPIO_Init(); MX_ThreadX_Init();`
`MX_ThreadX_Init` = `tx_kernel_enter();` — never returns on success.
`tx_application_define(VOID *first_unused)` is the only place to call `tx_*_create` before the scheduler starts multi-threaded execution.
