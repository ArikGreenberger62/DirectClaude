---
name: modem/eg915n
description: Quectel EG915N LTE modem skill — power-on sequence, SMS send/receive, TCP socket AT commands for TFL_CONNECT_2 board.
---

# Quectel EG915N Modem Skill

## Hardware (TFL_CONNECT_2 board)

| Signal         | MCU Pin | Direction | Notes                                     |
|----------------|---------|-----------|-------------------------------------------|
| MDM_USART2_TX  | PA2     | MCU→MDM   | AF7 — modem UART RX                       |
| MDM_USART2_RX  | PA3     | MDM→MCU   | AF7 — modem UART TX                       |
| MDM_USART2_CTS | PD3     | MDM→MCU   | AF7 — hardware flow control               |
| MDM_USART2_RTS | PD4     | MCU→MDM   | AF7 — hardware flow control               |
| MC60_GNSS_PWR  | PB8     | MCU→MDM   | OUT — HIGH to enable modem power supply   |
| MC60_PWRKEY    | PD7     | MCU→MDM   | OUT — pulse HIGH 600ms to power on/off    |
| MC60_PWR       | PD12    | MCU→MDM   | OUT — extra power rail (if fitted)        |
| P3V3_SW_EN     | PC11    | MCU       | OUT — HIGH to enable 3.3V switched supply |
| MC60_RI        | PD2     | MDM→MCU   | IN (EXTI) — ring indicator                |

## Power-On Sequence

```c
/* 1. Enable 3.3V supply (already HIGH from gpio.c MX_GPIO_Init — confirm) */
HAL_GPIO_WritePin(GPIOC, P3V3_SW_EN_Pin, GPIO_PIN_SET);

/* 2. Enable modem power supply */
HAL_GPIO_WritePin(GPIOB, MC60_GNSS_PWR_Pin, GPIO_PIN_SET);
HAL_Delay(200);   /* power stabilisation */

/* 3. Check if modem already booted (send AT, wait 500ms for OK) */
/* If no response → send PWRKEY pulse */

/* 4. PWRKEY pulse: HIGH for 600 ms, then LOW
 * Circuit assumption: MCU HIGH → transistor ON → modem PWRKEY pulled LOW
 * (active-low trigger; 500 ms minimum per EG915N datasheet)            */
HAL_GPIO_WritePin(GPIOD, MC60_PWRKEY_Pin, GPIO_PIN_SET);
HAL_Delay(600);
HAL_GPIO_WritePin(GPIOD, MC60_PWRKEY_Pin, GPIO_PIN_RESET);

/* 5. Wait for boot — up to 30 s */
/* URCs to wait for: "RDY", "+CFUN: 1", "+QIND: SMS DONE" */
```

**Note:** If `MC60_PWRKEY` circuit is direct drive (no inversion), swap pulse polarity:
set LOW for 600ms, then HIGH.

## AT Initialization Sequence

```
ATE0                      ← disable echo (essential)
AT+CMEE=1                 ← numeric extended errors
AT+CPIN?                  ← expect "+CPIN: READY"; wait up to 10 s
AT+CREG=1                 ← enable network registration URC
(poll AT+CREG? until stat=1 or 5, max 60 s)
AT+CSQ                    ← signal quality (optional, for trace)
AT+CMGF=1                 ← text mode SMS
AT+CSCS="IRA"             ← IRA charset (ASCII compatible)
AT+CSDH=0                 ← simplified +CMT headers
AT+CNMI=2,2,0,0,0         ← route incoming SMS directly to TE as +CMT URC
AT+CMGD=1,4               ← delete all stored messages (clean slate)
```

## SMS — Sending

```
AT+CMGS="<phone_number>"\r     ← note: \r not \r\n
wait for '>' prompt character
<message text>
\x1A                            ← Ctrl+Z to send (0x1A)
response: +CMGS: <mr>\r\n\r\nOK\r\n
timeout: 120 s (network dependent)
```

**Example AT sequence (complete):**
```
→ AT+CMGF=1
← OK
→ AT+CSCS="IRA"
← OK
→ AT+CMGS="+972501234567"\r
← > 
→ Hello World\x1A
← 
← +CMGS: 12
← 
← OK
```

## SMS — Receiving (URC)

Enabled by `AT+CNMI=2,2,0,0,0`.

**Format (text mode, CSDH=0):**
```
+CMT: "<sender>","","<timestamp>"\r\n
<message body>\r\n
```

**Parsing sender from +CMT header line:**
```c
/* Line: +CMT: "+972501234567","","23/01/01,12:00:00+08" */
char *p = strchr(line, '"');
if (p) {
    char *q = strchr(p + 1, '"');
    if (q) {
        size_t len = (size_t)(q - p - 1);
        strncpy(sender, p + 1, len);
        sender[len] = '\0';
    }
}
```

**CMTI alternative (CNMI=2,1):** Modem stores SMS and sends `+CMTI: "SM",<idx>`.
Then read with `AT+CMGR=<idx>` and delete with `AT+CMGD=<idx>`.

## TCP Socket

The EG915N uses the Quectel TCP/IP AT command set (separate manual). Key commands:

### Configure PDP Context
```
AT+QICSGP=1,1,"<APN>","","",1   ← contextID=1, TCP/IP, APN
```

### Activate Context
```
AT+QIACT=1                       ← activate context 1
AT+QIACT?                        ← verify: +QIACT: 1,1,1,"<ip>"
```

### Open TCP Socket
```
AT+QIOPEN=1,0,"TCP","<host>",<port>,0,1
←  +QIOPEN: 0,0           (connectID=0, err=0 = success)
URC arrives asynchronously after connection
```

Access mode=1 (direct push mode) means received data comes as:
```
+QIURC: "recv",0,<length>\r\n<data>
```

### Send Data
```
AT+QISEND=0,<length>            ← connectID=0
wait for '>' prompt
<data bytes>
← SEND OK
```

### Receive Data (buffer mode, access mode=0)
```
+QIURC: "recv",0                ← unsolicited: data available
AT+QIRD=0,1460                  ← read up to 1460 bytes
← +QIRD: <actual_length>
← <data>
← OK
```

### Close Socket
```
AT+QICLOSE=0                    ← connectID=0
← OK
```

### Deactivate Context
```
AT+QIDEACT=1
← OK
```

## Key URCs Summary

| URC                     | Meaning                              | When active          |
|-------------------------|--------------------------------------|----------------------|
| `RDY`                   | Module boot complete                 | Always after boot    |
| `+CFUN: 1`              | Full functionality available         | Always after boot    |
| `+CPIN: READY`          | SIM card ready                       | Always on SIM insert |
| `+CREG: <stat>`         | Network registration change          | AT+CREG=1            |
| `+QIND: SMS DONE`       | SMS subsystem ready                  | After SMS init       |
| `+CMT: ...`             | Incoming SMS (direct to TE)          | AT+CNMI=2,2,...      |
| `+CMTI: <mem>,<idx>`    | Incoming SMS stored in memory        | AT+CNMI=2,1,...      |
| `+QIURC: "recv",<cid>`  | TCP data available                   | TCP open, mode 0     |

## Common Gotchas

- **Echo must be off** (ATE0) before parsing AT responses — otherwise every command echoes back.
- **PWRKEY pulse polarity** depends on hardware circuit (often inverted via transistor).
- **Wait for "+QIND: SMS DONE"** before sending SMS or the first send may fail.
- **+CMT is two lines** — header `+CMT: ...` followed by message body on next `\r\n`.
- **Flow control**: USART2 has HW RTS/CTS; modem may assert CTS during busy periods.
- **SMS character set**: use `AT+CSCS="IRA"` for ASCII text from terminal, `"GSM"` for raw GSM7.
- **AT+CMGD=1,4 at startup** clears all stored messages; prevents "SMS full" errors.
- **Modem busy after SMS send**: wait for OK/ERROR before sending next command.
- After power-on, modem needs ~5–10 s to register on network (AT+CREG stat=1).
