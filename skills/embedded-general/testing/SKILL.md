---
name: embedded-general/testing
description: General embedded testing strategy. Read when setting up testing for firmware.
task_types: [test]
keywords: [test, verify, trace, ctest, mock, unit, host, serial, uart, com, baud, reset, flash, hardware, validate, check]
priority: base
---

# Embedded Firmware Testing

## Two-Track Testing

### Track 1 — Host unit tests (no hardware)
- Compile `app_*.c` with MSVC/MinGW (`-DBUILD_TESTS=ON`)
- Stub `drivers/*.c` with mock implementations using `#ifdef UNIT_TEST`
- Run: `ctest` after host build
- Validates logic before touching hardware

### Track 2 — Closed-loop trace testing (on hardware)
After flash, read the trace UART for 5–10 s and parse output.
Compare actual `[TAG] ...` lines against expected patterns.
If wrong or missing → diagnose → fix code → rebuild → reflash → re-read.
Only declare done when expected trace lines are confirmed.

## Closed-Loop Agent Workflow
1. Open serial port first (before MCU reset — startup messages are lost otherwise)
2. Trigger MCU reset via programmer CLI `-hardRst`
3. Read `ser.readline()` loop for 10 s
4. Parse tags programmatically
5. If mismatch → diagnose from trace → loop back to build

## Trace Port Rules
- Must remain available (not locked by other tools) during iterative cycles
- Open → reset MCU → read → close → fix → repeat
