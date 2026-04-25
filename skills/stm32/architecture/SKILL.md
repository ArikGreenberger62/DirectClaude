---
name: stm32/architecture
description: STM32 HAL + CubeMX project architecture. Read when designing a new STM32 project structure.
task_types: [arch, code, build]
keywords: [stm32, hal, cubemx, lowlevel, project, structure, architecture, msp, peripheral, override, ioc, h573, h5xx]
priority: tier2
---

# STM32 Project Architecture

## LowLevel — Single Source of Truth
- `LowLevel/` contains all CubeMX-generated HAL drivers and peripheral init
- New projects reference `LowLevel/` via CMake paths — do NOT copy HAL files
- When `.ioc` is regenerated → update `LowLevel/` first
- Project app code lives only in `Core/Src/` and `Core/Inc/`

## File Override Pattern (M1)
When a LowLevel file must be modified for a project:
1. Copy `LowLevel/Core/Src/<file>.c` → `Core/Src/<file>.c`
2. Edit the copy
3. List `Core/Src/<file>.c` in application sources block in CMakeLists.txt
4. **Remove** `${LL}/Core/Src/<file>.c` from LowLevel sources block
> Duplicate listing = duplicate symbol linker error. Project copy wins.

## HAL Usage Policy
- Always prefer `HAL_*` functions over direct register access
- Exception: direct register only when HAL overhead is measured to be a bottleneck
- Before writing a driver, check `STM32H5xx_HAL_Driver/Inc/` for existing HAL function

## Peripheral Architecture Notes
- PWM: `HAL_TIM_PWM_Init` only — never pair with `HAL_TIM_Base_Init` on same handle (M7)
- SPI Mode 3 (CPOL=1): always set `MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE` (M2)
- SPI STM32H5 HAL V1.6.0: do NOT use `HAL_SPI_TransmitReceive` polling — use direct-register transfers (M4)

## External Device Knowledge
For every external IC requiring a driver:
- Check `c-embedded/devices/<DatasheetName>.md` before writing the driver
- After writing/debugging, update the file with gotchas, working init sequences, errata
