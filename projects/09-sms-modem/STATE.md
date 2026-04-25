# 09-sms-modem — STATE (compact session cache)

> Auto-load file. Full archive: `CONTEXT.md`.
> Modem reference: `skills/modem/eg915n/SKILL.md` + `c-embedded/devices/`.

## Status
Build CLEAN — flash 81,040 B / 2 MB; RAM 8,152 B / 640 KB.
**Hardware VERIFIED: modem init PASS, SMS sent to +972544801489 — SUCCESS.**

## Key facts
- Modem: Quectel **EG915N** on **USART2 PA2/PA3** with HW CTS PD3 / RTS PD4.
- Power: P3V3_SW_EN PC11 HIGH → MC60_GNSS_PWR PB8 HIGH → MC60_PWR PD12 HIGH → PWRKEY PD7 1s pulse.
- Trace + commands on **UART7 PE7/PE8** @ 115200 (COM7).
- Byte-by-byte interrupt RX → 512-byte ring buffer → `Modem_Process()`.
- Blocking AT engine (`mdm_cmd`, `mdm_wait_for`) — no RTOS.

## SMS commands
- TX (UART7 → MCU → modem): `sendsms +<phone> , <message>`
- RX (modem → MCU → UART7): `sms received from +<phone> , <message>`

## Bug fixes locked in
1. **Callback pRxBuffPtr bug:** `HAL_UART_RxCpltCallback` was reading
   `*(pRxBuffPtr)` but HAL increments `pRxBuffPtr` past the byte before the
   callback fires. Fixed: `Modem_RxByte(void)` now reads `s_rx_byte` directly.
2. **CPIN match string:** was `"READY"` which would match `"+CPIN: NOT READY"`.
   Fixed to `"+CPIN: READY"` (no false positive on NOT READY).
3. **CPIN timeout:** single 10s shot failed; SIM takes ~15s after RDY.
   Fixed: poll loop up to 30s (3s per attempt, 2s between retries).
4. **MC60_PWR_Pin (PD12):** was left LOW by MX_GPIO_Init. Now set HIGH in
   `mdm_power_on()` — required for modem power on this board.

## Boot timing (observed)
- PWRKEY pulse → ~5s → RDY URC received.
- CTS=HIGH at 5s mark; modem asserts CTS before 12s.
- SIM ready: ~2-5s after RDY (CPIN: PASS on 1st retry after 2s).
- Network registration: fast (already registered).
- Total init time: ~20-25s from cold boot.

## AT init sequence (locked in)
`ATE0 → AT+CMEE=1 → AT+CPIN? (poll 30s) → AT+CREG? (poll 60s) →
AT+CSQ → AT+CMGF=1 → AT+CSCS="IRA" → AT+CSDH=0 →
AT+CNMI=2,2,0,0,0 → AT+CMGD=1,4`

## Next step
1. Receive-side test: send SMS from phone → expect `sms received from … , …` on UART7.
2. (Optional) Remove `[MODEM] diag: CTS=...` prints once confident in hardware.
