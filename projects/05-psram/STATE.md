# 05-psram — STATE (compact session cache)

> Auto-load file. Compact summary of where this project stands.
> Full archive: `CONTEXT.md`.

## Status
DONE 2026-04-12. Verified on hardware. Frozen.
Build: flash 80 KB / 2 MB; RAM 6.8 KB / 640 KB.

## Key facts
- IS66WVS16M8FBLL 128 Mb PSRAM via SPI2 @ 60 MHz, Mode 0.
- FAST READ (0x0B) + chunked write/read (≤20 B/CS to honour tCEM = 4 µs @85°C).
- Direct-register SPI transfer pattern (port of project 04 `spi1_xfer`).
- Software CS on PE13 (LowLevel does not configure it).
- UART7 interrupt RX command parser:
  `WriteRAM(addr,len,hh hh ...)` / `ReadRAM(addr,len)` →
  `RAM_DATA(0xAAAAAA,NN,HH HH ... HH)`.
- Startup self-test writes 8-byte pattern @ 0x000000.

## Bug fix locked in
**SPI2 prescaler workaround** — `HAL_SPI_Init` does not apply MBR bits on SPI2
(STM32H5 HAL V1.6.0). After `MX_SPI2_Init()`:
```c
CLEAR_BIT(SPI2->CR1, SPI_CR1_SPE);
MODIFY_REG(SPI2->CFG1, SPI_CFG1_MBR, SPI_BAUDRATEPRESCALER_4);
```
Verify with `[REG]` dump — CFG1 must read `0x10070007`.

## Next step
Frozen.

## See also
- `c-embedded/devices/IS66WVS16M8FBLL.md` (or equivalent knowledge file).
