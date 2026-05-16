# Quectel FC41D — Wi-Fi 802.11b/g/n + Bluetooth 5.2 Combo Module

## Overview

- **Chipset:** MCU-based, up to 120 MHz processor
- **WiFi:** IEEE 802.11b/g/n (2.4 GHz only)
- **BLE:** Bluetooth 5.2 (central + peripheral)
- **Interface:** UART (AT commands) + SPI (data)
- **Datasheet:** `Quectel_FC41D&FCM100D&FCM740D&FLMx40D_AT_Commands_Manual_V2.0.pdf`

## TFL_CONNECT_2 Wiring

- UART9 (PD14/PD15) for AT commands
- SPI4 (PE2/4/5/6) for high-speed data
- PD5 = WIFI_BLE_PWR_EN, PD6 = WIFI_BLE_RESETN (active low, open drain)
- PD11 = BLE_STS input

## Key AT Commands

See `skills/wifi-ble/fc41d/SKILL.md` for full reference.

### WiFi
- `AT+QWSCAN` — scan APs
- `AT+QSTAAPINFO=<ssid>,<key>` — connect to AP
- `AT+QSTAAPINFODEF=<ssid>,<key>` — connect + save
- `AT+QGETWIFISTATE` — check connection

### TCP/UDP
- `AT+QIOPEN=<cid>,"TCP","<host>",<port>,<lport>,<mode>` — open socket
- `AT+QISEND=<cid>,<len>` — send data (wait for `>`)
- `AT+QIRD=<cid>,<maxlen>` — read buffered data
- `AT+QICLOSE=<cid>` — close socket

### BLE
- `AT+QBLEINIT=<role>` — 1=central, 2=peripheral
- `AT+QBLESCAN=1/0` — start/stop scan
- `AT+QBLEADVPARAM=<min>,<max>` — advertising interval
- `AT+QBLEADVDATA=<hex>` — set advertising payload (max 31 bytes)
- `AT+QBLEADVSTART` / `AT+QBLEADVSTOP`

## Known Issues / Errata

- RSSI from `AT+QWSCAN` is offset: `actual_dBm = returned_value - 100`
- `AT+QBLECFGMTU` and `AT+QBLETRANMODE` require firmware ≥ FC41DAAR03A07
- GATT services (`AT+QBLEGATTSSRV`) can only be set once after `AT+QBLEINIT=2`; must deinit first to change
- Socket error 551 ("unknown error") reported on forums — check WiFi is connected before opening TCP socket

## BLE Scan URC Format — Firmware FC41DAAR03A09 (confirmed on hardware 2026-05-16)

The SKILL.md and AT Commands Manual describe:
```
+QBLESCAN: AA:BB:CC:DD:EE:FF,0,-55,0201...
```
(space after colon; MAC with colons; RSSI; adv_data)

**Actual format from FC41DAAR03A09M02.bin firmware:**
```
+QBLESCAN:<rssi_or_empty>,<addr_type>,<addr_12hex_no_colons>[,<adv_hex>]
```
Example: `+QBLESCAN:,1,1f953971eae5`

Key differences:
- **No space** after the colon
- **RSSI field may be empty** (no value before first comma)
- **MAC is 12 lowercase hex chars without colons**, NOT `AA:BB:CC:DD:EE:FF`
- `AT+QBLESCAN?` returns `+QBLESCAN:1` (scan active) or `+QBLESCAN:0` (not active) — same prefix, distinguishable by 12-char addr check in parser

Parser must check for `"+QBLESCAN:"` (no space) and convert addr from `1f953971eae5` → `1f:95:39:71:ea:e5`.
