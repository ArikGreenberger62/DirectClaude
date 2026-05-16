# 13-wifi-scan — STATE

## Status
Build clean. Flashed (2026-05-16). Firmware running and verified.
FC41D detection PASS. WiFi scan runs correctly; returns SCAN_NO_AP (0 networks in current environment).

## Key facts
- `FC41D_WifiScan(aps, max_aps)` sends AT+QWSCAN, waits up to 18 s, returns AP count.
- `parse_qwscan()` uses `strrchr` to split last comma → SSID vs BSSID (BSSID always 17 chars).
- RSSI: `ap.rssi = (int8_t)(raw_signal - 100)` where raw is 0–63 from module.
- Scan repeats every 30 s; IWDG refreshed before each scan.
- Results printed as aligned table via UART7 at 115200 baud (COM7).
- FC41D on UART9: PD14=RX, PD15=TX, GPIO_AF11. Power: PD5=PWR_EN, PD6=RESETN (open-drain).
- FC41D firmware: FC41DAAR03A09M02.bin_202406260343

## Verified trace (2026-05-16)
```
[WIFI-SCAN] 13-wifi-scan starting
[FC41D] attempt 1: ATE0... ERROR   <- expected (echo-on boot quirk)
[FC41D] attempt 1: AT...   OK
[FC41D] detect: PASS - FC41D responding
[FC41D] ver: +QVERSION:FC41DAAR03A09M02.bin_202406260343
[WIFI-SCAN] FC41D: PASS - module detected
[FC41D] AT+QWSCAN (WiFi scan, up to 18 s)...
[UART9] +QSTASTAT:SCAN_NO_AP    <- no APs in range; firmware handles correctly
[WIFI-SCAN] No networks found
```

## Bug fixes locked in
- No local main.h wrapper — `#include "main.h"` resolves to ST_IOT via include path.
- `fc41d_getline` timeout per call = FC41D_SCAN_TIMEOUT (18 s) — needed because scan can take 5–10 s before first line arrives.

## Next step
Move board near a WiFi router to verify AP table output. Optionally extend with AT+QWSCAN mode flags (channel list, passive scan) to improve scan coverage.
