# IS66/67WVS16M8FALL/BLL — ISSI 128Mb Serial PSRAM

## Device Overview
- **Part**: IS66WVS16M8FBLL-104K (BLL = 3.0V, 104MHz speed grade)
- **Capacity**: 128Mb (16MB), 16M × 8-bit
- **Interface**: SPI (1-1-1) and QPI (4-4-4)
- **SPI Mode**: Mode 0 (CPOL=0, CPHA=0), data sampled on rising CLK edge
- **VDD**: 2.7V – 3.6V (BLL variant); compatible with 3.3V MCU supplies
- **Max clock**: 33MHz for READ (0x03), 104MHz for FAST READ (0x0B) and WRITE (0x02)
- **Address**: 24-bit (A23:A0), byte-addressable
- **Page size**: 1024 bytes (burst wraps within page)
- **Die structure**: dual-die stack of two 64Mb dice; die boundary at 0x800000

## Connection on TFL_CONNECT_2
- **SPI bus**: SPI2 (CAN_SPI2) — PB14=MISO, PB15=MOSI, PA12=SCK
- **Chip Select**: PE13 (PSRAM_SPI2_NSS_Pin) — active LOW, software GPIO

## Key Commands (SPI mode)

| Code | Command       | Frame (SPI)                        | Max Freq |
|------|---------------|------------------------------------|----------|
| 0x03 | READ          | CMD + A23:0 + DATA out             | 33 MHz   |
| 0x0B | FAST READ     | CMD + A23:0 + 1 dummy byte + DATA  | 104 MHz  |
| 0x02 | WRITE         | CMD + A23:0 + DATA in              | 104 MHz  |
| 0x9F | READ ID       | CMD + A23:0(dc) + MFR + KGD + EID  | 104 MHz  |
| 0x66 | RESET ENABLE  | CMD only                           | 104 MHz  |
| 0x99 | RESET         | CMD only (must follow 0x66)        | 104 MHz  |

## READ ID Register (0x9F)
Full-duplex frame: send [0x9F, 0x00, 0x00, 0x00, 0xFF, 0xFF], read [_, _, _, _, MFR, KGD]
- **rx[4]** = Manufacturer ID = **0x9D** (ISSI)
- **rx[5]** = KGD = **0x5D** (Pass) or 0x55 (Fail — device not tested/failed)
- rx[6..11] = EID (density + reserved bits; density bits[47:45]: 100 = 128Mb)

## Power-Up Sequence
1. After VDD stable, device needs **150µs** self-initialization before any command.
2. After 150µs, issue RESET ENABLE (0x66) + RESET (0x99) for a clean start.
3. CS must be HIGH for at least 1 CLK period between operations (tCPH).

## Critical Timing Constraint — tCEM
**CE# Maximum Low Time**: 4µs at 85°C, 1µs at 105°C.
The PSRAM is DRAM-based and needs CE# to return HIGH periodically for self-refresh.
→ **Must chunk long transfers**: at 60MHz SPI, max ≈ 25 total bytes per CS assertion.
→ Driver uses 20-byte data chunks per CS cycle (header=4B write / 5B fast-read + 20B data).

## Working Init Sequence
```c
// 1. Assert CS HIGH (deasserted) before any access
// 2. Wait 1ms (> 150µs power-up)
// 3. Send RESET ENABLE: cs_low, tx[0x66], cs_high
// 4. Send RESET: cs_low, tx[0x99], cs_high
// 5. Wait 1ms (reset completion)
// 6. Read ID: tx=[0x9F,0,0,0,0xFF,0xFF], rx[4]=MFR(0x9D), rx[5]=KGD(0x5D)
// 7. Write/read verify: write pattern, read back, compare
```

## Working Driver Notes
- Use **direct-register SPI transfers** (not HAL_SPI_TransmitReceive) — avoids STM32H5 HAL V1.6.0 FIFO bug.
- SPI2 prescaler=2 → 60MHz; use FAST READ (0x0B) with 1 dummy byte, not slow READ (0x03).
- SPI Mode 0 (CPOL=0, CPHA=0) — no spurious edge issue with AFCNTR=0.
- `NSSPMode = SPI_NSS_PULSE_DISABLE` since CS is driven by software GPIO (PE13).
- Chunk size = **20 data bytes** per CS assertion for tCEM compliance at 60MHz:
  - Write chunk: 4B header + 20B data = 24B × (8/60MHz) = 3.2µs < 4µs ✓
  - Read chunk: 5B header + 20B data = 25B × (8/60MHz) = 3.33µs < 4µs ✓
- Die boundary at 0x800000 — do not cross in a single burst (not an issue for small test buffers).
- FAST READ dummy byte: 8 wait cycles = 1 extra byte (0xFF) after 3-byte address.

## Known Errata / Surprises
- (2026-04-11) First driver implementation. tCEM chunk requirement confirmed in practice.
- (2026-04-12) **SPI overclocking corrupts ID reads.** At 120 MHz (SPI2 prescaler=2 instead of intended =4), READ_ID returned MFR=0xDF KGD=0x7F instead of 0x9D/0x5D. Root cause: HAL_SPI_Init did not apply the prescaler (STM32H5 HAL bug). After forcing prescaler=4 (60 MHz) via direct register write, ID and self-test passed immediately.
- (2026-04-12) **Hardware verified working** at 60 MHz (PCLK1 240 MHz / prescaler 4). WriteRAM/ReadRAM tested successfully with pattern DE AD BE EF at address 0x001000.
