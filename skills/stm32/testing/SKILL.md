---
name: stm32/testing
description: STM32 flash and debug workflow. Read when flashing firmware or debugging with ST-LINK.
task_types: [test]
keywords: [stm32, flash, st-link, stlink, programmer, swd, gdb, gdbserver, debug, hardrst, reset, download, programmer_cli, h573]
priority: tier2
---

# STM32 Flash & Debug

## Tool Paths
| Tool | Path |
|---|---|
| STM32_Programmer_CLI | `C:\Users\arikg\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe` |
| ST-LINK GDB Server | `C:\Users\arikg\AppData\Local\stm32cube\bundles\stlink-gdbserver\7.13.0+st.3\bin\ST-LINK_gdbserver.exe` |

## Flash Procedure
**Step 1 — Detect ST-LINK first (always):**
```
STM32_Programmer_CLI.exe -l st
```
If no ST-LINK found → report clearly and stop. Do NOT attempt flash.

**Step 2 — Flash (SWD, reset after):**
```
STM32_Programmer_CLI.exe -c port=SWD freq=4000 -w build/Debug/<project>.elf -rst
```

**Only flash when the user explicitly asks** or when running the full autonomous loop.

## Hard Reset (for trace capture)
```
STM32_Programmer_CLI.exe -c port=SWD freq=4000 -hardRst
```
Use this to reset the MCU after opening the serial port, so startup messages are not missed.

## GDB Debug Session
- Server: `ST-LINK_gdbserver.exe --swd --port-number 61234 --device STM32H573VITxQ`
- Client: `arm-none-eabi-gdb.exe`
- VSCode: configured via `launch.json` (see stm32/build skill)
