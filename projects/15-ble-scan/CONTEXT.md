## Current State
Build clean, flashed, trace verified on hardware (2026-05-16).
FC41D detection works. BLE scan (`AT+QBLEINIT=1` + `AT+QBLESCAN=1`) runs 5 s.
Found 0 devices at test time (no BLE hardware in range). All AT exchanges confirmed.

## Architecture Decisions
- Built on project 12's `fc41d.c`/`ring_buf.c` foundation (interrupt-driven UART9 RX)
- Extended fc41d.c with `FC41D_BLE_Scan()`: collects `+QBLESCAN:` URCs for N ms,
  deduplicates by MAC, extracts device name from AD structure (type 0x08/0x09)
- Live progress printed during scan (`[BLE-SCAN] found #N: addr rssi name`)
- Summary printed at end of scan with device index, MAC, addr_type, RSSI, name
- IWDG refreshed inside the scan collection loop every ~200 ms
- `static BLE_Device_t devices[20]` in main — avoids 1.1 KB stack hit

## Last Session
- Created all files from scratch (2026-05-16): CMakeLists.txt, CMakePresets.json,
  Core/Src/{main.c, fc41d.c, ring_buf.c, stm32h5xx_it.c}, Core/Inc/{fc41d.h, ring_buf.h}
- Build: clean first attempt, 0 errors, 0 warnings (48 compile units)
- Flash + trace: FC41D detected (ATE0→ERROR, AT→OK, version FC41DAAR03A09M02)
  BLE init OK, scan ran 5 s, 0 devices (expected — no BLE in range)
- Next: test with BLE devices present to confirm URC parsing and printing
