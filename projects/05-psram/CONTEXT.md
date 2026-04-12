## Current State

**Done:**
- PSRAM driver (psram.c): RESET_EN/RESET, READ_ID, Write (chunked), Read (FAST READ chunked)
- SPI2 at 60 MHz (PLL1Q 120 MHz / prescaler 2), Mode 0, software CS on PE13
- UART7 interrupt-driven command parser: WriteRAM(addr,len,hh hh ...) / ReadRAM(addr,len)
- Response format: RAM_DATA(0xAAAAAA,NN,HH HH ... HH)
- LED: GREEN blink = PSRAM OK + self-test pass; RED blink = failure
- Startup self-test: writes 8-byte pattern to 0x000000, reads back, compares
- Builds cleanly: 0 errors, 0 warnings (-Wall -Wextra -Werror)
- Flash: 80KB/2MB, RAM: 6.8KB/640KB

**All done — verified on hardware.**

## Architecture Decisions

- **SPI2 at 60 MHz (prescaler=2)**: tCEM = 4 µs at 85°C → ~30 bytes max per CS.
  Prescaler=8 (15MHz) only allows ~7 bytes — too tight for header+useful data.
  60 MHz: 20-byte data chunks → write=24B=3.2µs, read=25B=3.33µs, both < 4µs.
- **FAST READ (0x0B)** used instead of READ (0x03): READ is limited to 33 MHz;
  at 60 MHz only FAST READ (with 1 dummy byte) is valid.
- **Direct-register SPI transfers** (not HAL_SPI_TransmitReceive): avoids STM32H5
  HAL V1.6.0 FIFO bug. Pattern copied from project 04 lsm6dso.c → spi1_xfer().
- **PE13 as software GPIO CS**: LowLevel gpio.c does not configure PE13. Initialized
  in PSRAM_NSS_Init() in main.c (GPIOE clock already on from MX_GPIO_Init).
- **SPI IRQ + DMA disabled after init**: MspInit links DMA channels 5/6 to SPI2
  and enables SPI2_IRQn. Immediately disabled in main.c + hdmatx/hdmarx = NULL.
- **Mode 0 (CPOL=0) + AFCNTR=0**: safe, no spurious edge. M2 rule only applies
  to CPOL=1 (Mode 2/3). MasterKeepIOState=DISABLE is correct here.
- **spi.c copied from LowLevel** (M1 rule): only SPI2 prescaler and NSSPMode
  changed; SPI1 and SPI4 kept as-is for completeness (not called from main).

## Last Session (2026-04-12)

- Finished: Flashed and verified on real hardware. All trace output correct.
- Bug found & fixed: HAL_SPI_Init was not applying SPI2 prescaler (CFG1 MBR bits
  stayed at 0 = div-2 → 120 MHz, overclocking the PSRAM). Added direct-register
  MODIFY_REG workaround in main.c after MX_SPI2_Init. Root cause unknown — the HAL
  WRITE_REG at line 414 of stm32h5xx_hal_spi.c writes the correct value but bit 28
  does not stick. With the workaround, CFG1=0x10070007 (div-4 = 60 MHz) confirmed.
- Verified: WriteRAM(0x001000,4,DE AD BE EF) → ReadRAM(0x001000,4) →
  RAM_DATA(0x001000,4,DE AD BE EF) ✓
- Project complete.
