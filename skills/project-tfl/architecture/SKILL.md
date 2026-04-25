---
name: project-tfl/architecture
description: TFL_CONNECT_2 board-specific architecture. Read when starting a new project in this workspace.
task_types: [arch, build]
keywords: [tfl, tfl_connect, project, new, board, structure, h573, ioc, peripheral, lowlevel, layout, checklist]
priority: tier3
---

# TFL_CONNECT_2 — Project Architecture

## Board & MCU
- **MCU:** STM32H573VIT3Q (LQFP100, Cortex-M33, no TrustZone)
- **HAL Package:** STM32Cube FW_H5 V1.6.0
- **RTOS:** Azure RTOS ThreadX + FileX

## Workspace Layout
```
DirectClaude/
├── CLAUDE.md               ← this master file (skill router)
├── TFL_CONNECT_2_H573.ioc  ← CubeMX config
├── LowLevel/               ← CubeMX-generated sources (source of truth)
│   ├── Core/Src/           ← gpio.c, usart.c, spi.c, etc.
│   ├── Drivers/            ← HAL + CMSIS
│   └── STM32H573xx_FLASH.ld
├── cmake/
│   └── arm-none-eabi.cmake ← shared toolchain
├── c-embedded/
│   ├── SKILL.md            ← (legacy — superseded by skills/)
│   └── devices/            ← per-IC knowledge files
├── skills/                 ← this skill hierarchy
└── projects/
    ├── 01-blink/
    ├── 04-gsensor/         ← canonical reference project
    └── ...
```

## Canonical Reference
**Always examine `projects/04-gsensor/`** before starting a new project — it is the reference for CMake layout, file structure, and LowLevel integration patterns.

## Peripherals Configured in IOC
| Peripheral | Notes |
|---|---|
| ADC1 | 9 channels, DMA, analog watchdog |
| FDCAN1, FDCAN2 | CAN-FD bus |
| GPDMA1, GPDMA2 | General-purpose DMA |
| I2C1 | |
| IWDG | Watchdog |
| RTC | |
| SPI1, SPI2, SPI4 | |
| TIM3, TIM6, TIM7 | |
| UART4, UART7, UART9 | |
| USART1, USART2, USART3 | |

## New Project Checklist
Create in one pass — do not ask "should I create X?":
- [ ] `CMakeLists.txt`
- [ ] `CMakePresets.json`
- [ ] `cmake/arm-none-eabi.cmake` (copy from `../../cmake/`)
- [ ] `.vscode/settings.json`, `launch.json`, `tasks.json`
- [ ] `Core/Inc/*.h` + `Core/Src/*.c`
- [ ] LowLevel overrides (copy + edit; remove LL reference in CMake)
- [ ] `CONTEXT.md`
