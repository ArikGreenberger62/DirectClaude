# 15-ble-scan — STATE (compact session cache)

## Status
**DONE.** Build clean. Flashed and verified 2026-05-19.
8 BLE devices detected (all type=1 random/RPA — phones in privacy mode).
GATT name resolution skips type=1 devices (always reject connections). rssi=N/A shown for empty RSSI field.
Trace port: COM4 (FTDI USB Serial Port) — not COM7.

## Key facts
- FC41D on UART9 (PD14/PD15 AF11); trace on UART7 (PE7/PE8 AF7)
- Power-on: PD5=HIGH (PWR_EN), PD6=HIGH (release RESETN open-drain), 3 s boot wait
- ATE0 always returns ERROR on first boot (echo on) — fallback to AT, then retry ATE0
- BLE scan sequence: AT+QBLEINIT=0 (deinit, ignore error) → AT+QBLEINIT=1 → AT+QBLESCAN=1 → collect URCs → AT+QBLESCAN=0
- ACTUAL URC format (FC41DAAR03A09): `+QBLESCAN:<rssi_or_empty>,<addr_type>,<addr_12hex_no_colons>`
  e.g. `+QBLESCAN:,1,1f953971eae5`  — NO space after colon, NO colons in addr, RSSI may be empty
- RSSI field is always empty in FC41DAAR03A09 firmware → rssi_valid=0 → display "rssi=N/A"
- Parser auto-detects field order: if field1 is 12 or 17 chars → addr first (manual format);
  otherwise → RSSI first (actual firmware format). Both 12-char and 17-char colon MAC formats accepted.
- AT+QBLESCANPARAM is unsupported on FC41DAAR03A09 — returns ERROR (non-fatal, passive scan still works)
- Non-flushing AT probes sent every 5 s during scan window (response visible as [RAW] lines)
- IWDG refreshed every ~200 ms inside scan loop
- Scan duration: BLE_SCAN_DURATION_MS = 30000U (configurable in main.c)
- Deduplication by MAC addr (up to BLE_SCAN_MAX_DEVICES = 20 unique entries)
- GATT name resolution: only attempted for type=0 (public addr) devices
  AT+QBLECONN=0,<type>,<addr> → wait +QBLECONN:0,0 URC → AT+QBLEGATTCRD=0,3,0 → parse +QBLEGATTRD data → AT+QBLEDISCONN=0
- FC41D radio sensitivity is lower than a phone's BLE radio. Devices must be within ~1–2 m.

## Bug fixes locked in
- ATE0 fallback: attempt ATE0 first, if ERROR fall back to AT, then re-send ATE0
- CRITICAL: Actual URC prefix is "+QBLESCAN:" (NO space). Old parser used "+QBLESCAN: " (with space) → rejected every URC
- AT+QBLEINIT=0 deinit before AT+QBLEINIT=1 — clears stale BLE state on warm resets
- MAC address in URC is 12 lowercase hex chars without colons; parser formats to XX:XX:XX:XX:XX:XX
- Parser handles both field orders (RSSI-first and addr-first) and both MAC formats (colon and no-colon)
- GATT skips type=1 devices (random/RPA — phones always reject connections)
- GATT connect timeout: after 8 s with no +QBLECONN URC, send AT+QBLEDISCONN=0 before returning
- GATT connect ERROR: reinit BLE stack (AT+QBLEINIT=0 → AT+QBLEINIT=1) before returning
- rssi_valid flag added to BLE_Device_t — distinguishes empty RSSI (N/A) from true 0 dBm

## Next step
To test with the Samsung SmartTag (type=0 public addr — GATT works):
  1. Place the SmartTag within 20 cm of the TFL_CONNECT_2 board
  2. Press the SmartTag button to trigger an advertising burst
  3. Reset and run — SmartTag (addr_type=0) will appear and GATT name resolved
Next extension options: RSSI filtering by threshold, scan for specific MAC prefix, advertise as peripheral.
