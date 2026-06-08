# 16-ble-beacon — CONTEXT

## Current State
Project created 2026-05-18. Source complete, not yet built.

Goal: BLE beacon using FC41D module. Advertises with device name "CCoreAIBLE"
and custom manufacturer-specific payload containing "Hello from CCoreAI".

## Architecture Decisions

**Why manufacturer-specific data for the message:**
The advertising payload is capped at 31 bytes. Including both the complete name
(12 bytes for "CCoreAIBLE") and the full 18-byte message "Hello from CCoreAI"
(22 bytes including overhead) would require 37 bytes — exceeding the limit.
Decision: set the name via AT+QBLENAME (appears in GATT and BLE scan responses),
and use the 31-byte advertising packet entirely for flags (3 bytes) + the message
as manufacturer-specific data (22 bytes). Total: 25 bytes, well within limit.

**Why company ID 0xFFFF:**
0xFFFF is listed as "not assigned" in the Bluetooth SIG registry and is
commonly used for prototype/internal manufacturer-specific data.

**Why no AT+QBLEGATTSSRV:**
A beacon only needs to advertise — no GATT service is required. Adding
GATT services would complicate the init sequence without benefit.

**Reuse from project 15:**
- ring_buf.h/c: identical (UART9 byte-by-byte RX)
- stm32h5xx_it.c: identical
- fc41d.c: Init and Detect functions identical; FC41D_BLE_Scan replaced with
  FC41D_BLE_BeaconStart (peripheral role instead of central)

## Last Session (2026-05-18)
- Created all source files in one pass
- Build pending — not yet configured or compiled
- Next: cmake --preset Debug, cmake --build, flash, trace verify
