# Project 02 — UART7 Line Echo

## Goal

Test UART7 at 115200 8N1. Receive characters until CR or LF, then echo the
accumulated line back wrapped in double-quotes followed by `\r\n`.

Example: send `hello` + Enter → receive `"hello"\r\n`

## Hardware

| Signal | Pin | AF |
|---|---|---|
| UART7_RX | PE7 (H_CON_UART7_RX) | AF7 |
| UART7_TX | PE8 (H_CON_UART7_TX) | AF7 |

UART7 clock source: APB3 = 240 MHz (default bus clock, no special peripheral clock remap)

## Design

- **Receive:** interrupt-driven, one byte at a time via `HAL_UART_Receive_IT`.
- **Buffer:** 256-byte static buffer accumulates characters.
- **Echo trigger:** CR (`\r`) or LF (`\n`) flushes the buffer.
- **CRLF handling:** if the buffer is empty when CR/LF arrives, nothing is sent
  (handles the second byte of a CRLF pair silently).
- **Transmit:** blocking `HAL_UART_Transmit` from the main loop via a flag
  set in the RX callback (avoids blocking inside the ISR context).

## Key files

| File | Purpose |
|---|---|
| `Core/Src/main.c` | Clock config, UART7 init, echo state machine |
| `Core/Src/stm32h5xx_hal_msp.c` | PE7/PE8 GPIO AF7, NVIC priority 5 |
| `Core/Src/stm32h5xx_it.c` | `UART7_IRQHandler` → `HAL_UART_IRQHandler` |

## Build

```
cmake --preset Debug
cmake --build --preset Debug
```

Output: `build/Debug/uart7-echo.elf` (~20 KB flash, ~2.2 KB BSS)
