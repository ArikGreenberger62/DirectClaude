# 16-ble-beacon — STATE (compact session cache)

## Status
**BUILD CLEAN.** Flash pending — ST-LINK probe found (SN 38FF7006304E4B3057262543) but SWD returns
"Unable to get core ID". Physical connection to board needs checking before flash.
Flash size: 77 KB / 2 MB (3.7%). RAM: 7 KB / 640 KB (1.1%).

## Key facts
- FC41D on UART9 (PD14/PD15 AF11); trace on UART7 (PE7/PE8 AF7)
- Power-on: PD5=HIGH (PWR_EN), PD6=HIGH (release RESETN open-drain), 3 s boot wait
- ATE0 always returns ERROR on first boot (echo on) — fallback to AT, then retry ATE0
- BLE name: "CCoreAIBLE" (set via AT+QBLENAME, appears in GATT + scan responses)
- Advertising payload (25 bytes):
    02 01 06 = Flags (LE General Discoverable, no BR/EDR)
    15 FF FF FF = Manufacturer Specific, company=0xFFFF, len=21
    48 65 6C 6C 6F 20 66 72 6F 6D 20 43 43 6F 72 65 41 49 = "Hello from CCoreAI"
- AT+QBLEADVDATA hex string: 02010615FFFFFF48656C6C6F2066726F6D2043436F72654149
- Advertising interval: 100 ms (AT+QBLEADVPARAM=160,160)
- Beacon sequence: QBLEINIT=0 → QBLEINIT=2 → QBLENAME → QBLEADVPARAM → QBLEADVDATA → QBLEADVSTART
- LED: green 1 Hz blink = beacon running, red solid = FC41D not detected, red fast blink = BLE init failed
- Status trace every 10 s: "[STATUS] beacon running — advertising CCoreAIBLE / Hello from CCoreAI"

## Bug fixes locked in
- ATE0 fallback from project 15: attempt ATE0, if ERROR fall back to AT, then retry ATE0
- AT+QBLEINIT=0 deinit before QBLEINIT=2 — clears stale BLE state

## Next step
1. Check physical SWD connection to board (ST-LINK SN 38FF7006304E4B3057262543 returns "Unable to get core ID")
2. Once connection restored: `STM32_Programmer_CLI.exe -c port=SWD freq=4000 -w build/Debug/ble_beacon.elf -rst`
3. Trace verify on COM7 @ 115200: expect [BEACON] ... PASS lines, then [STATUS] every 10 s
4. Confirm with a phone BLE scanner: device "CCoreAIBLE" visible, manufacturer data shows "Hello from CCoreAI"
