# Project 03 — gsensor

## Goal

Read LSM6DSO accelerometer via SPI1, perform two-position rotation-matrix calibration, trace raw and calibrated XYZ at 1 Hz on USART1.

## Hardware

| Signal     | Pin  | Notes                             |
|---|---|---|
| SPI1 SCK   | PA5  | AF5, shared bus (ACC + NVM)       |
| SPI1 MISO  | PA6  | AF5                               |
| SPI1 MOSI  | PA7  | AF5                               |
| ACC NSS    | PE9  | Software GPIO, active-low         |
| LED_R      | PC8  | GPIO, fast blink during calibrate |
| LED_G      | PC9  | GPIO, slow blink when done        |
| P3V3_SW_EN | PC11 | Must be HIGH                      |
| USART1 TX  | PA9  | AF7, 115200 8N1 trace             |

## Design

See `CONTEXT.md` for architecture decisions.

### LSM6DSO config
- ±8G full scale, 104 Hz ODR
- CTRL1_XL = 0x6C, CTRL3_C = 0x44
- SPI Mode 3 (CPOL=1, CPHA=1), 7.5 MHz

### Calibration sequence
1. Board stable → sample position 1 (32 samples, stability check)
2. Red LED fast (5 Hz) → user rotates 90°
3. Board stable at new position → sample position 2
4. Compute R via Gram-Schmidt; verify det(R)≈1
5. OK → green blink (1 Hz); FAIL → stop LEDs, retry in 3 s

### Trace format (USART1)
```
RAW  Xr=±NNNNN Yr=±NNNNN Zr=±NNNNN
CAL  Xc=±NNNNN Yc=±NNNNN Zc=±NNNNN
```
Scale: 1024 = 1G. Z_cal ≈ +1024 when board flat (Z down).

## Build

```
cmake --preset Debug
cmake --build --preset Debug
```

Output: `build/Debug/gsensor.elf`

## Device datasheet

Knowledge file: `c-embedded/devices/lsm6dso.md`
