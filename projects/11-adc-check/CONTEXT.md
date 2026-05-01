# 11-adc-check — CONTEXT

## Current State
Build clean. Flash and trace verify not yet done.

## Architecture Decisions
- Full LowLevel peripheral set included (same pattern as 04-gsensor) to avoid
  missing weak symbol issues; only ADC1 + UART7 are actually used at runtime.
- DMA buffer is `volatile uint32_t g_adc_buf[9]` in main.c; snapshot taken by
  direct array read in the 5s loop (32-bit reads are atomic on Cortex-M33).
- No interrupt-driven scheme needed — continuous circular DMA keeps the buffer
  fresh; polling the snapshot every 5 s is sufficient.
- mV conversion uses integer arithmetic (raw * 3300 / 4095) — no float needed.

## Last Session (2026-05-01)
- Created project from scratch based on 04-gsensor scaffold.
- Fixed: HAL_ADCEx_Calibration_Start takes 2 args in FW_H5 V1.6.0, not 3.
- Build: clean at 86 KB flash / 6 KB RAM.
- Next: flash to board and verify [ADC] trace lines on COM7.
