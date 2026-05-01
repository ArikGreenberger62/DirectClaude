# 11-adc-check — STATE (compact session cache)

## Status
Build clean (0 errors, 0 warnings). Flash + trace verify pending.

## Key facts
- ADC1 DMA circular, 9 channels (32-bit per slot)
- DMA buffer indices: [1]=PVIN (PA4/CH18/rank2), [2]=Ignition (PC0/CH10/rank3)
- `HAL_ADCEx_Calibration_Start` takes 2 args in FW_H5 V1.6.0 (no CalibrationMode arg)
- Trace: UART7 115200 8N1, once every 5 s, two columns (raw counts + mV)
- mV = raw * 3300 / 4095 (assumes 3.3 V reference; actual voltages depend on dividers)

## Bug fixes locked in
- `HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED)` — 2 args, not 3

## Next step
Flash and verify trace output on COM7.
