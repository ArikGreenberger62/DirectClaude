# 12-ble-detect — STATE (compact session cache)

## Status
Build clean. Flashed. Trace verified PASS.
FC41D firmware: FC41DAAR03A09M02.bin_202406260343

## Key facts
- Power seq: P3V3_SW_EN (PC11) already HIGH from MX_GPIO_Init; WIFI_BLE_PWR_EN (PD5) driven HIGH; WIFI_BLE_RESETN (PD6) released HIGH (open-drain); 3 s boot wait
- AT comm: UART9 (PD14/PD15 AF11) @ 115200 8N1; interrupt-driven RX via ring buffer
- ATE0 returns ERROR on first attempt (echo on, echo of "ATE0" isn't "OK"), plain AT returns OK; ATE0 then succeeds on second pass
- Version query: AT+QVERSION → +QVERSION:FC41DAAR03A09M02.bin_202406260343
- No local main.h — source files get ST_IOT's main.h directly via include path; LED #defines are local to main.c only

## Bug fixes locked in
- Do NOT add a local main.h that uses `../../ST_IOT/Core/Inc/main.h` relative include — it resolves to the wrong path (project-root/ST_IOT/...) and fails to compile. Instead, omit local main.h and let the -I${LL}/Core/Inc path deliver ST_IOT's main.h directly.
- ATE0 on a module with echo enabled returns ERROR (the echo "ATE0" is read as a line before "OK"); fall back to plain AT to confirm liveness, then send ATE0 again.

## Next step
Project is complete (detection confirmed). Optional: extend to WiFi scan or BLE scan using the FC41D skill.
