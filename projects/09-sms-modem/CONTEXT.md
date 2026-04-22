# 09-sms-modem — Project Context

## Current State

**Build:** CLEAN (0 errors, 0 warnings)
- Flash: 80 064 B / 2 MB (3.82%)
- RAM:    8 144 B / 640 KB (1.24%)

**Flash:** Probe detected (SN: 38FF7006304E4B3057262543). Previous DEV_CONNECT_ERR session unresolved — requires user to flash.

**Test:** Not yet done (awaiting flash + trace).

---

## Architecture Decisions

| Decision | Reason |
|---|---|
| USART2 (PA2/PA3) with HW CTS/RTS (PD3/PD4) | Board wiring; reuses LowLevel MX_USART2_UART_Init which already sets UART_HWCONTROL_RTS_CTS |
| UART7 (PE7/PE8) for trace/commands | Free port with FTDI on COM7 |
| Byte-by-byte interrupt RX (HAL_UART_Receive_IT 1-byte, re-armed in callback) | Simplest ISR-safe pattern; avoids DMA complexity |
| 512-byte ring buffer (ring_buf.c) | Decouples ISR from processing; sized for worst-case SMS URC |
| Blocking AT engine (mdm_cmd/mdm_wait_for) | No RTOS in this project; blocking with HAL_GetTick timeout is correct |
| Two-line +CMT parsing via s_expect_sms_body flag | EG915N in text mode sends header line then body line; state flag in Modem_Process() handles split |
| P3V3_SW_EN (PC11) driven HIGH in mdm_power_on | Controls 3.3V switched supply for modem VCC path |
| MC60_GNSS_PWR (PB8) HIGH then MC60_PWRKEY (PD7) 600 ms pulse | Standard Quectel power-on sequence (transistor inverts PWRKEY) |

---

## Hardware Pin Map

| Signal | Pin | Direction |
|---|---|---|
| USART2_TX | PA2 | OUT to modem RX |
| USART2_RX | PA3 | IN from modem TX |
| USART2_CTS | PD3 | IN (modem RTS) |
| USART2_RTS | PD4 | OUT (modem CTS) |
| P3V3_SW_EN | PC11 | OUT HIGH = 3.3V on |
| MC60_GNSS_PWR | PB8 | OUT HIGH = modem VCC on |
| MC60_PWRKEY | PD7 | OUT HIGH pulse = power key |
| LED_R | PC8 | OUT |
| LED_G | PC9 | OUT |
| Trace UART7_RX | PE7 | IN (from USB-UART) |
| Trace UART7_TX | PE8 | OUT (to USB-UART) |

---

## AT Init Sequence (mdm_at_init)

1. `ATE0` — disable echo
2. `AT+CMEE=1` — numeric error codes
3. `AT+CPIN?` — verify SIM present
4. `AT+CREG?` / poll until stat=1 or 5 (60 s max)
5. `AT+CSQ` — log signal quality
6. `AT+CMGF=1` — SMS text mode
7. `AT+CSCS="IRA"` — ASCII charset
8. `AT+CSDH=0` — hide header details in +CMT
9. `AT+CNMI=2,2,0,0,0` — route new SMS to DTE via +CMT URC
10. `AT+CMGD=1,4` — delete all stored SMS

---

## SMS Command Format (UART7)

```
sendsms +<phone> , <message>
```

Example:
```
sendsms +972501234567 , Hello World
```

## SMS Receive Output (UART7)

```
sms received from +972501234567 , Hello World
```

---

## Last Session

### Finished
- Trace UX improvements:
  - On `Modem_Init` PASS, print multi-line "SMS COMMAND GUIDE" block with usage,
    rules, example, and incoming-SMS format.
  - `Modem_SendSMS` now prints a framed request echo (Destination / Message /
    Length) before dialling, and a clear `RESULT: SUCCESS` or
    `RESULT: FAIL - <reason>` line re-stating phone and message on completion.
  - Enlarged tracef buffer to 256 B to fit 160-char message + prefix.
  - `main.c` unknown-command and bad-format hints now show usage + example.
- Clean build (0 errors, 0 warnings). Flash 78 392 B → 80 064 B.

### Currently Broken / Incomplete
- Hardware flash + trace verification not performed this session (code-only
  change). Previous DEV_CONNECT_ERR has not been re-tried.

### Next Step
1. Flash:
   `STM32_Programmer_CLI.exe -c port=SWD freq=4000 -w build/Debug/sms_modem.elf -rst`
2. Open COM7 @ 115200 — confirm the new "SMS COMMAND GUIDE" block prints after
   `[MODEM] init: PASS - modem READY`.
3. Type `sendsms +972XXXXXXXXX , Hello World` — confirm framed echo and
   `RESULT: SUCCESS` (or `FAIL - <reason>` with the phone+message repeated).
4. Receive-side still works unchanged: external SMS → `sms received from ... , ...`.
