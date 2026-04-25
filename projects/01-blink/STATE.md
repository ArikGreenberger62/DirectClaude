# 01-blink — STATE (compact session cache)

## Status
DONE. TIM3 PWM on LED_R (PC8/CH3) + LED_G (PC9/CH4), 1 Hz 50 % duty.

## Key facts
- Self-contained — does NOT use `LowLevel/`. HAL pulled from
  `STM32Cube_FW_H5_V1.6.0` directly.
- PWM via `HAL_TIM_PWM_Init` only — never paired with `HAL_TIM_Base_Init`
  (M7 rule: pairing skips MspInit → GPIO AF + clock never configured).

## Next step
Frozen.
