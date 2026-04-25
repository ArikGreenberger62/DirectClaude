---
name: project-tfl/testing
description: TFL_CONNECT_2 closed-loop trace testing. Read when verifying firmware on hardware.
task_types: [test]
keywords: [tfl, trace, uart7, com7, serial, pyserial, verify, test, hardrst, self_test, closed-loop]
priority: tier3
---

# TFL_CONNECT_2 — Trace Verification

## Trace Port
- **UART:** UART7 (PE7=RX, PE8=TX), **115200 8N1**
- **Host COM port:** COM7
- Must remain unlocked (not held by another terminal) during test cycles

## Python Trace Reader
```python
import serial, time, subprocess

# Step 1: open port BEFORE reset (capture startup messages)
ser = serial.Serial('COM7', 115200, timeout=5)
time.sleep(0.5)

# Step 2: hard-reset MCU
subprocess.run([
    r"C:\Users\arikg\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe",
    "-c", "port=SWD", "freq=4000", "-hardRst"
], capture_output=True)

# Step 3: read for 10 s
deadline = time.time() + 10
lines = []
while time.time() < deadline:
    line = ser.readline().decode('utf-8', errors='replace').strip()
    if line:
        print(line)
        lines.append(line)
ser.close()
```

## Expected Trace Format
```
[MODULE] description: PASS
[MODULE] description: FAIL - reason
[REG] SPI CR1=0x... CFG1=0x... CFG2=0x... SR=0x...
RAW  ax=... ay=... az=...
CAL  ax=... ay=... az=...
```

## Pass / Fail Criteria
- All `[MODULE] ... PASS` lines present
- No `FAIL` lines
- Runtime tag lines (`RAW`, `CAL`, etc.) appearing at ~1 Hz
- No unexpected silence after reset

## If Test Fails
1. Read trace — identify first `FAIL` or missing tag
2. Check register dump lines for wrong config values
3. Fix code → rebuild → reflash → re-read trace
4. Repeat until all expected lines confirmed

## SELF_TEST Mode
Build with `-DSELF_TEST` for startup hardware validation:
- SYSCLK frequency check
- RAM write/read pattern
- IWDG running (read SR)
- UART transmit (no HAL error)
- Add peripheral-specific tests per project
