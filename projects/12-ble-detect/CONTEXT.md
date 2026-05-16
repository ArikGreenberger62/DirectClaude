# 12-ble-detect — CONTEXT

## Current State
Done. Build clean, flashed, trace verified PASS.
FC41D responds to AT commands via UART9. Firmware version FC41DAAR03A09M02.

## Architecture Decisions
- Reused ring_buf + AT driver pattern from 09-sms-modem, adapted for FC41D power sequence
- No local main.h override — ST_IOT main.h reached via include path (-I${LL}/Core/Inc)
- Blocking TX (HAL_UART_Transmit) for commands; interrupt-driven RX (HAL_UART_Receive_IT) for responses
- Detection window: 10 s total, polling every 500 ms; falls through to AT if ATE0 fails (echo-on behaviour)

## Last Session (2026-05-16)
- Built project from scratch, modelled on 09-sms-modem
- Fixed: relative `#include "../../ST_IOT/Core/Inc/main.h"` in local main.h does not resolve correctly; solution: no local main.h
- Trace confirmed: WIFI_BLE_PWR_EN HIGH + WIFI_BLE_RESETN released → 3 s boot → ATE0 (ERROR, echo on) → AT (OK) → AT+QVERSION returns version string
- Next: WiFi scan or BLE scan project can build on this detection skeleton
