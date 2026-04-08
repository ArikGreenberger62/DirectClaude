# LSM6DSO — IMU Device Knowledge

**Datasheet:** lsm6dso.pdf (ST DocID027517)
**Device:** 6-axis IMU — 3-axis accel + 3-axis gyro, SPI/I2C interface
**WHO_AM_I:** 0x6C

---

## SPI Interface

- 4-wire SPI, MSB first
- Supports **Mode 0 (CPOL=0, CPHA=0) and Mode 3 (CPOL=1, CPHA=1)**
- **Max SPI clock: 10 MHz** (use 7.5 MHz or lower to be safe)
- NSS: active-low, software-controlled GPIO
- **Read:** assert CS, send `0x80 | reg_addr`, read N bytes, deassert CS
- **Write:** assert CS, send `reg_addr` (bit7=0), send data byte(s), deassert CS
- Multi-byte: address auto-increments if `CTRL3_C.IF_INC = 1` (default after reset = 1; set explicitly in CTRL3_C=0x44)

## Key Registers

| Addr | Name       | Reset | Description                                      |
|---|---|---|---|
| 0x0F | WHO_AM_I   | 0x6C  | Device ID — must read 0x6C                       |
| 0x10 | CTRL1_XL   | 0x00  | Accel ODR [7:4], FS [3:2], LPF2 [1]              |
| 0x12 | CTRL3_C    | 0x04  | BDU [6], SW_RESET [0], IF_INC [2]                |
| 0x28 | OUTX_L_A   | —     | Accel X low byte (burst: X_L, X_H, Y_L, Y_H, Z_L, Z_H) |

### CTRL1_XL — Accelerometer config

| Bits  | Field   | Value | Meaning              |
|---|---|---|---|
| [7:4] | ODR_XL  | 0110  | 104 Hz normal mode   |
| [3:2] | FS_XL   | 00    | ±2G                  |
|       |         | 10    | ±4G                  |
|       |         | 11    | ±8G ← **use this**   |
|       |         | 01    | ±16G                 |
| [1]   | LPF2    | 0     | LPF2 disabled        |

**For ±8G, 104 Hz:** `CTRL1_XL = 0x6C` (0b0110_1100)

### CTRL3_C — Control 3

| Bit | Field   | Value | Meaning                          |
|---|---|---|---|
| 6   | BDU     | 1     | Block data update (recommended)  |
| 2   | IF_INC  | 1     | Auto-increment register address  |
| 0   | SW_RESET| 0     | Normal operation                 |

**Recommended init value:** `CTRL3_C = 0x44` (BDU=1, IF_INC=1)

## Sensitivity

| Full Scale | Sensitivity (mg/LSB) | 1G in counts |
|---|---|---|
| ±2G        | 0.061                | ~16393        |
| ±4G        | 0.122                | ~8197         |
| ±8G        | 0.244                | ~4098 ≈ **4096** |
| ±16G       | 0.488                | ~2049         |

**Scale for ±8G → output in units where 1024 = 1G:** `output = raw >> 2`

## Init Sequence (verified working on TFL_CONNECT_2)

```c
/* 1. Verify WHO_AM_I */
reg_read(0x0F) → must return 0x6C

/* 2. Configure CTRL3_C: BDU + auto-increment */
reg_write(0x12, 0x44)

/* 3. Configure CTRL1_XL: 104 Hz, ±8G */
reg_write(0x10, 0x6C)

/* 4. Wait ≥10 ms for first sample */
HAL_Delay(15)
```

## Accelerometer Data Registers

Starting at 0x28, burst-read 6 bytes:
- `raw[0]` = OUTX_L_A, `raw[1]` = OUTX_H_A → `x = (raw[1]<<8) | raw[0]`
- `raw[2]` = OUTY_L_A, `raw[3]` = OUTY_H_A → `y = (raw[3]<<8) | raw[2]`
- `raw[4]` = OUTZ_L_A, `raw[5]` = OUTZ_H_A → `z = (raw[5]<<8) | raw[4]`

## Known Gotchas / Errata

- **DO NOT exceed 10 MHz SPI clock** — sensor may misbehave silently; use prescaler 16 (7.5 MHz) with 120 MHz APB2.
- **Power-up settling:** Wait at least **25 ms** after P3V3 rail enable before first SPI access — 10 ms minimum per datasheet but 25 ms observed necessary on TFL_CONNECT_2. Add `HAL_Delay(25U)` after `MX_GPIO_Init()` (where P3V3_SW_EN is asserted), before SPI peripheral init. Missing this delay causes WHO_AM_I read to fail and `LSM6DSO_Init` to return error immediately.
- **BDU is important:** Without BDU=1, you may read an X_H byte from one sample and an X_L from the next at high ODR rates. Always set CTRL3_C.BDU=1.
- **IF_INC default=1 after reset, but reset may not always occur** — set it explicitly in CTRL3_C.
- **NSS timing:** Hold NSS low for the entire transaction; de-assert between separate transactions. The sensor does not support continuous NSS assertion across unrelated commands.
- **SPI clock on TFL_CONNECT_2:** PA5 = SCK, PA6 = MISO, PA7 = MOSI — all AF5. NSS = PE9, GPIO output active-low.

## Project Usage

- **03-gsensor:** ±8G, 104 Hz, blocking SPI reads, two-position calibration.
