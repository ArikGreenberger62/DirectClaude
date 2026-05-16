# 15-ble-scan — STATE (compact session cache)

## Status
Build clean (0 errors, 0 warnings). Flashed and parser-fix verified 2026-05-16.
Scan finds devices when BLE advertisers are nearby (confirmed: phone MAC 1f953971eae5
appeared in diagnostic run). Scan returns 0 when no advertisers in range.

## Key facts
- FC41D on UART9 (PD14/PD15 AF11); trace on UART7 (PE7/PE8 AF7)
- Power-on: PD5=HIGH (PWR_EN), PD6=HIGH (release RESETN open-drain), 3 s boot wait
- ATE0 always returns ERROR on first boot (echo on) — fallback to AT, then retry ATE0
- BLE scan sequence: AT+QBLEINIT=0 (deinit, ignore error) → AT+QBLEINIT=1 → AT+QBLESCAN=1 → collect URCs → AT+QBLESCAN=0
- ACTUAL URC format (FC41DAAR03A09): `+QBLESCAN:<rssi_empty>,<addr_type>,<addr_12hex_no_colons>`
  e.g. `+QBLESCAN:,1,1f953971eae5`  — NO space after colon, NO colons in addr, RSSI may be empty
- AT+QBLESCAN? returns `+QBLESCAN:1` (scan active) — same prefix, rejected by parser (addr not 12 chars)
- Parser converts 12-hex addr to XX:XX:XX:XX:XX:XX format
- Non-flushing AT probes sent every 5 s during scan window (response visible as [RAW] lines)
- [RAW] logging active during scan window — all module lines logged for debugging
- IWDG refreshed every ~200 ms inside scan loop
- Scan duration: BLE_SCAN_DURATION_MS = 15000U (configurable in main.c)
- Deduplication by MAC addr (up to BLE_SCAN_MAX_DEVICES = 20 unique entries)

## Bug fixes locked in
- ATE0 fallback: attempt ATE0 first, if ERROR fall back to AT, then re-send ATE0
- CRITICAL: Actual URC prefix is "+QBLESCAN:" (NO space). Old parser used "+QBLESCAN: " (with space) → rejected every URC → 0 devices always
- AT+QBLEINIT=0 deinit before AT+QBLEINIT=1 — clears stale BLE state on warm resets
- MAC address in URC is 12 lowercase hex chars without colons (not AA:BB:CC:DD:EE:FF format)

## Next step
Test with BLE devices in range. Bring phone with Bluetooth ON and discoverable (or Android phone
with any BLE app like nRF Connect advertising). Reset board and observe [BLE] lines in trace.
