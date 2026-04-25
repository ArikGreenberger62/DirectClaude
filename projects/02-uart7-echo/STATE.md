# 02-uart7-echo — STATE (compact session cache)

## Status
DONE. UART7 line echo at 115200 8N1 — receives a line, echoes it with quotes.

## Key facts
- Trace UART = UART7 PE7 (RX) / PE8 (TX), AF7. (Not AF11.)
- Interrupt-driven RX: `HAL_UART_Receive_IT(&huart7, &byte, 1)` re-armed
  in `HAL_UART_RxCpltCallback`. Blocking TX runs from main loop, never
  from the callback.

## Next step
Frozen.
