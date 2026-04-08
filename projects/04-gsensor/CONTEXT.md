# 04-gsensor — Project Context

## Current State

**Build verified clean. Flashed and working. Calibration functional.**

Build output (Debug):
- Flash: 79,612 B / 2 MB (3.80%)
- RAM:    6,400 B / 640 KB (0.98%)
- ELF: `build/Debug/gsensor.elf`

**Hardware status observed on serial terminal (UART7, COM7, 115200 8N1):**
- `[REG] SPI1 CFG2=0x87400000` — AFCNTR=1 (fix for SPI clock glitch)
- `[REG] CTRL3_C=0x44, CTRL1_XL=0x6C` ✓ — sensor registers configured correctly
- `[GSENSOR] LSM6DSO OK — WHO_AM_I=0x6C, ±8G, 104 Hz` ✓
- `[CAL] Position 1 captured` ✓ — calibration starts automatically
- `[MOVE] rx=-57 ry=365 rz=-4072` — reading ~-1G on Z when flat ✓

## Architecture Decisions

### Maximum LowLevel reuse
All CubeMX-generated files from `../../LowLevel/` are referenced directly in CMakeLists.txt
without copying. Only three files are modified and copied into the project:
- `Core/Src/spi.c` — SPI1 changed to Mode 3 (CPOL=HIGH, CPHA=2EDGE), prescaler 16 (7.5 MHz),
  NSSPMode disabled, **MasterKeepIOState=ENABLE (AFCNTR=1)** — critical fix.
- `Core/Src/stm32h5xx_it.c` — identical to LowLevel + `__errno` stub (nano.specs requirement).
- `Core/Src/main.c` — uses LowLevel's SystemClock_Config and MX_* init prototypes;
  adds LED_R/LED_G init (not in LowLevel gpio.c), gsensor application loop.

### SPI1 AFCNTR fix (root cause of init failure)
**CRITICAL DISCOVERY:** With CPOL=1 (SPI Mode 3), SCK idle state is HIGH. When SPE=0
and AFCNTR=0, SCK goes high-impedance and floats LOW (no pull-up). Each time SPE enables
for a transfer, SCK snaps LOW→HIGH = spurious rising clock edge. The sensor interprets
this as a data bit, shifting all received data by 1 position. This caused:
- CTRL3_C default (0x04) reading as 0x08 (shifted left 1 bit)
- WHO_AM_I (0x6C) sometimes reading as 0x00 (frame desync)
- All register writes appearing to fail (sensor saw wrong addresses/data)
Fix: `MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE` → AFCNTR=1 in SPI_CFG2.
This makes the SPI peripheral hold SCK at CPOL level even when SPE=0.

### Direct-register SPI transfers
The lsm6dso.c driver uses a custom `spi1_xfer()` function with direct register access
instead of `HAL_SPI_TransmitReceive()`. The STM32H5 HAL V1.6.0 polling SPI function
has FIFO-drain issues with small 8-bit transfers (RXWNE handler unconditionally reads
4 bytes). The direct-register approach is simpler and more reliable.

### SPI1 DMA/IRQ disconnect
LowLevel MspInit configures DMA channels and SPI1 IRQ for SPI1. Since we use polling
(direct register) only, main.c disables SPI1_IRQn and GPDMA1 Channel 1/2 IRQs, and
nulls out hdmatx/hdmarx pointers to prevent any interference.

### SPI1 clock source
CCIPR3=0 (PCLK2 selected), not PLL1Q as MspInit intends.
Both give 120 MHz → 7.5 MHz at prescaler 16. Sensor works either way.

### HAL timebase via TIM4
LowLevel uses TIM4 (not SysTick) as HAL tick source via `stm32h5xx_hal_timebase_tim.c`.
`SysTick_Handler` in `stm32h5xx_it.c` is empty. `HAL_TIM_PeriodElapsedCallback` calls
`HAL_IncTick()` when `htim->Instance == TIM4`.

### LED_R/LED_G not in LowLevel gpio.c
PC8 and PC9 are not in the IOC, so CubeMX did not generate their init.
`LED_Init()` in main.c configures them as push-pull outputs after `MX_GPIO_Init()`.

### Calibration algorithm
Identical to project 03: two-position Gram-Schmidt rotation matrix.
Calibration state machine: SAMPLE_POS1 → WAIT_MOVE → WAIT_STABLE2 → SAMPLE_POS2 → COMPUTE → DONE/FAILED.
Uses `__builtin_sqrtf`/`__builtin_fabsf` (needs `-lm` link flag + `__errno` stub).

## Last Session

### What we finished
- Identified and fixed the root cause of SPI communication failure:
  **AFCNTR=0 with CPOL=1 causes spurious clock edge on every SPE enable**
- Fix: `MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE` in spi.c
- Replaced HAL SPI polling with direct-register `spi1_xfer()` (bypasses H5 HAL FIFO bug)
- Disabled SPI1 DMA/IRQ (not needed for polling)
- Verified sensor init: WHO_AM_I=0x6C, CTRL3_C=0x44, CTRL1_XL=0x6C
- Verified accelerometer readings: Z≈-4072 (~-1G when flat)
- Calibration starts and captures position 1 automatically

### What is currently working
- SPI1 communication (reads and writes)
- LSM6DSO init with SW reset, BDU, IF_INC, 104Hz ODR, ±8G full scale
- Calibration position 1 auto-capture
- Movement detection diagnostic prints every 2s
- LED_R fast blink during WAIT_MOVE state

### Next step
1. Physically rotate board 90° to complete calibration (position 2)
2. Verify calibration PASS and 1 Hz trace output
3. If trace works, remove the `[MOVE]` diagnostic prints and [REG] dump
4. Final clean build + flash
