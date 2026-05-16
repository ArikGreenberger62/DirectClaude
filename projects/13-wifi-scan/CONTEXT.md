# 13-wifi-scan — CONTEXT

## Goal
Scan for available WiFi networks using the Quectel FC41D module on TFL_CONNECT_2
and print the list to UART7 (COM7 at 115200 baud).

## Architecture Decisions

- **FC41D driver**: Extended from project 12 (BLE detect).
  - Added `FC41D_ApInfo_t` struct (rssi, security, ssid, bssid).
  - Added `FC41D_WifiScan(aps, max_aps)` — sends `AT+QWSCAN`, parses
    `+QWSCAN: <signal>,<security>,<ssid>,<bssid>` lines, returns AP count.
  - `parse_qwscan()` splits SSID from BSSID using `strrchr` on the last comma.
    BSSID is always 17 chars; everything before the last comma is the SSID.

- **Main loop**: Init → Detect → scan every 30 s.
  - IWDG refreshed before each scan (scan can block up to 18 s).
  - Results printed in an aligned table to UART7.

- **Ring buffer**: 512-byte ISR-safe circular buffer (unchanged from proj 12).

## Key AT Command
```
AT+QWSCAN
```
Response: one `+QWSCAN: <signal>,<security>,<ssid>,<bssid>` line per AP, then `OK`.
Signal range 0–63 maps to dBm as: `actual_dBm = signal - 100`.
Security: 0=OPEN, 1=WEP, 2=WPA, 3=WPA2, 4=WPA/WPA2 mixed.

## Hardware
| Signal            | Pin  | Notes                          |
|-------------------|------|--------------------------------|
| WIFI_UART9_TX     | PD15 | MCU→FC41D (AF11)               |
| WIFI_UART9_RX     | PD14 | FC41D→MCU (AF11)               |
| WIFI_BLE_PWR_EN   | PD5  | HIGH to enable module power    |
| WIFI_BLE_RESETN   | PD6  | Open-drain, HIGH = released    |

## Last Session
- Project created from scratch (based on 12-ble-detect).
- All source files written; build pending.
- Next step: cmake configure + build, fix any warnings/errors.
