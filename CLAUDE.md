# DirectClaude — Shared Project Workspace

This workspace contains a series of embedded firmware projects for the **TFL_CONNECT_2** board, all derived from a common STM32CubeMX `.ioc` configuration.

## Hardware Reference

**MCU:** STM32H573VIT3Q (LQFP100, Cortex-M33, no TrustZone)
**IOC file:** `TFL_CONNECT_2_H573.ioc` (in this root folder)
**Toolchain:** STM32CubeIDE / GCC
**HAL Package:** STM32Cube FW_H5 V1.6.0
**RTOS:** Azure RTOS ThreadX + FileX

## General Rules (apply to every project)

### 1. Toolchain — use what is installed on this machine

Always use the STM32CubeIDE 1.19.0 toolchain paths:

| Tool | Path |
|---|---|
| STM32CubeIDE | `C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\stm32cubeide.exe` |
| arm-none-eabi-gcc | `C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-gcc.exe` |
| arm-none-eabi-gdb | same `tools\bin\` folder, `arm-none-eabi-gdb.exe` |
| make | `C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.100.202601091506\tools\bin\make.exe` |

- GCC version: **14.3.rel1**
- Do NOT suggest downloading or installing any other toolchain version.
- When writing `Makefile` or CMake rules, reference these paths.

### 2. HAL — use the firmware package installed on this machine

Always use the locally installed HAL:

```
C:\Users\arikg\STM32Cube\Repository\STM32Cube_FW_H5_V1.6.0\
  Drivers\
    STM32H5xx_HAL_Driver\   ← HAL source and headers
    CMSIS\                   ← CMSIS core + device headers
    BSP\                     ← Board Support Package
  Middlewares\               ← ThreadX, FileX, etc.
```

- Prefer HAL functions (`HAL_*`) over direct register access unless HAL is too slow for the use case.
- Before writing a driver, check if a HAL function already exists in `STM32H5xx_HAL_Driver\Inc\`.
- Use the CMSIS headers from this package for core definitions (`stm32h5xx.h`, etc.).
- Do NOT copy HAL source files into project — reference them via include paths pointing to the repository above.

### 3. CMake + VSCode — project build system

Every project must contain a `CMakeLists.txt` and `CMakePresets.json` that work with VSCode cmake-tools and the ST VSCode extension.

**Tools installed on this machine:**

| Tool | Path |
|---|---|
| cmake | `C:\Users\arikg\AppData\Local\stm32cube\bundles\cmake\4.0.1+st.3\bin\cmake.exe` |
| ninja | `C:\Users\arikg\AppData\Local\stm32cube\bundles\ninja\1.13.1+st.1\bin\ninja.exe` |

**Required VSCode extensions (already installed):**
- `ms-vscode.cmake-tools`
- `stmicroelectronics.stm32cube-ide-build-cmake`
- `stmicroelectronics.stm32-vscode-extension`
- `stmicroelectronics.stm32cube-ide-debug-stlink-gdbserver`
- `mcu-debug.peripheral-viewer`, `mcu-debug.rtos-views`

**Every project folder structure:**
```
projects/NN-name/
├── CMakeLists.txt          ← main CMake (editable)
├── CMakePresets.json       ← cmake-tools integration
├── CONTEXT.md              ← project map: current state, arch decisions, last session (Rule 9)
├── cmake/
│   └── arm-none-eabi.cmake ← toolchain file
├── .vscode/
│   ├── settings.json       ← points cmake-tools to local cmake/ninja
│   ├── launch.json         ← GDB debug via ST-LINK GDB server
│   └── tasks.json          ← build task
├── Core/
│   ├── Inc/
│   └── Src/
└── build/                  ← gitignored, cmake output
```

**`CMakePresets.json` must define:**
- `configurePreset` with generator `Ninja`, toolchain file `cmake/arm-none-eabi.cmake`
- `buildPreset` for `Debug` and `Release`
- cmake and ninja paths from bundles above

**`cmake/arm-none-eabi.cmake` toolchain file must set:**
- `CMAKE_SYSTEM_NAME = Generic`
- `CMAKE_C_COMPILER` = arm-none-eabi-gcc from Rule 1
- CPU flags: `-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard`

**`.vscode/settings.json` must override:**
```json
{
  "cmake.cmakePath": "C:/Users/arikg/AppData/Local/stm32cube/bundles/cmake/4.0.1+st.3/bin/cmake.exe",
  "cmake.generator": "Ninja",
  "cmake.configureOnOpen": true
}
```

### 4. Compile and verify — every project must build cleanly

After creating or modifying any project:
1. Run `cmake --preset Debug` (or equivalent) to configure
2. Run `cmake --build --preset Debug` to compile and link
3. Build must complete with **0 errors and 0 warnings** (treat warnings as errors: `-Wall -Wextra -Werror`)
4. Verify the `.elf` output exists and `arm-none-eabi-size` reports a sane flash/RAM usage
5. Do not consider a project done until this passes

### 5. Host-side testing — test without hardware

Separate logic from HAL so it can be compiled and run on the host (Windows x64):

- Put all business logic in `Core/Src/app_*.c` files with no direct HAL calls
- HAL calls live in thin `drivers/*.c` wrappers — these are excluded from host builds
- Add a `tests/` folder in each project with a native CMake target (`-DBUILD_TESTS=ON`)
- In test builds, stub the HAL wrappers with simple mock implementations
- Use `#ifdef UNIT_TEST` to swap in mocks
- Run tests with `ctest` after a host build

**Host build preset in `CMakePresets.json`** — use system MSVC or MinGW, not arm-none-eabi.

### 6. SELF_TEST — firmware startup self-test

When the firmware is compiled with `-DSELF_TEST`, it runs a startup self-test sequence before entering the main application loop. This validates the hardware after download.

**Rules:**
- `SELF_TEST` is a CMake option (`option(SELF_TEST "Run hardware self-test on startup" OFF)`)
- In `main.c`, call `SelfTest_Run()` immediately after all `MX_*_Init()` calls and before the main loop
- `SelfTest_Run()` lives in `Core/Src/self_test.c` / `Core/Inc/self_test.h`
- Results are reported over **USART1** (assumed as debug UART) at 115200 baud
- If any test fails, blink an error LED and halt (do not proceed to app)
- Output format: `[SELF_TEST] <module>: PASS` or `[SELF_TEST] <module>: FAIL - <reason>`

**Minimum self-test checklist for every project:**
```
[SELF_TEST] SYSCLK:   verify HAL_RCC_GetSysClockFreq() == expected
[SELF_TEST] RAM:      write/read pattern to a scratch buffer in RAM
[SELF_TEST] FLASH:    verify CRC of a known section (optional)
[SELF_TEST] IWDG:     verify watchdog is running (read SR)
[SELF_TEST] UART:     transmit known string, check no HAL error
```
Add peripheral-specific tests per project (e.g. ADC self-calibration result, SPI loopback if wired).

### 8. Code structure — small files and clear interfaces

**Small files:**
- Keep each `.c` file focused on a single responsibility — one module, one driver, one subsystem.
- If a function does not fit on a screen, split it.
- Aim for files under ~300 lines; if a file grows beyond that, split by sub-responsibility.

**Clear interfaces:**
- Every module must have a matching `.h` header that fully describes what the module does — public types, constants, and function prototypes only.
- Headers must be self-contained and include-guarded.
- No `extern` variables in headers; expose state only through accessor functions.
- A reader should understand the module's contract from the header alone, without reading the `.c` file.

### 9. Project Map — per-project CONTEXT.md

Every project folder must contain a `CONTEXT.md` file. This is the AI's short-term memory for that project and must be kept current.

**Required sections:**

```markdown
## Current State
High-level summary of what is finished and what is not.

## Architecture Decisions
Why specific libraries, patterns, or partitioning choices were made.

## Last Session
- What we just finished
- What is currently broken or incomplete
- What the next step is
```

**Rules:**
- Update `CONTEXT.md` at the end of every work session.
- When starting a new session on a project, read `CONTEXT.md` first before reading any source files.
- Add the file to the project folder structure under `projects/NN-name/CONTEXT.md`.

### 10. External device knowledge files

For every external component (Flash memory IC, BLE module, GPS receiver, cellular modem, sensor, etc.) that requires a driver:

- Create a knowledge file at `c-embedded/devices/<DatasheetName>.md` where `<DatasheetName>` is the exact filename of the component's datasheet PDF (without the `.pdf` extension).
- Example: datasheet is `W25Q128JV.pdf` → knowledge file is `c-embedded/devices/W25Q128JV.md`.
- **Before writing a driver**, check if the file exists and read it — it may already contain useful findings.
- **After writing or debugging a driver**, update the file with anything discovered (gotchas, working init sequences, timing constraints, errata, register values that differ from the datasheet, etc.).
- The file must include at minimum: device overview, key registers/commands, working code snippets or init sequence, and any known errata or surprises.

### 7. ST-LINK — firmware download

**Tools installed on this machine:**

| Tool | Path |
|---|---|
| STM32_Programmer_CLI | `C:\Users\arikg\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe` |
| ST-LINK GDB Server | `C:\Users\arikg\AppData\Local\stm32cube\bundles\stlink-gdbserver\7.13.0+st.3\bin\ST-LINK_gdbserver.exe` |

**Download command (SWD, reset after flash):**
```
STM32_Programmer_CLI.exe -c port=SWD freq=4000 -w <project>.elf -rst
```

**Detect ST-LINK before download:**
```
STM32_Programmer_CLI.exe -l st
```
Run this first. If no ST-LINK is found, report clearly and stop — do not attempt flash.

**`.vscode/launch.json` debug config must use:**
- `ST-LINK_gdbserver.exe` with `--swd` flag and the device name `STM32H573VITxQ`
- `arm-none-eabi-gdb.exe` from Rule 1 toolchain

**Only download when the user explicitly asks.** Always run `-l st` first to confirm ST-LINK is present.

### Peripherals configured in IOC
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

## Shared Skills

The `c-embedded/SKILL.md` file in this directory contains shared coding best practices. Read it before writing any C firmware code in any project.

@c-embedded/SKILL.md

## External Device Knowledge Base

Device-specific knowledge files live in `c-embedded/devices/`. Each file is named after the component's datasheet PDF (e.g., `W25Q128JV.md` for `W25Q128JV.pdf`). Read the relevant file before writing a driver; update it after. See Rule 10 for details.

## Projects

Projects live in numbered subfolders. Each has its own `CLAUDE.md` for project-specific context.

| # | Folder | Goal | Status |
|---|---|---|---|
| 01 | `projects/01-blink` | TIM3 PWM blink LED_R (PC8/CH3) + LED_G (PC9/CH4) at 1 Hz 50% | Done ✓ |
| 02 | `projects/02-uart7-echo` | UART7 line echo — 115200 8N1, echoes received line with quotes | Done ✓ |
| 03 | `projects/03-gsensor` | LSM6DSO ±8G via SPI1 + two-position calibration + 1 Hz trace | Done ✓ |
| 04 | `projects/04-gsensor` | Same as 03 but built on LowLevel CubeMX-generated sources (max reuse) | Done ✓ |

## Lessons Learned (cross-project knowledge)

This section is updated after each project. It captures patterns, mistakes, and discoveries that apply to all future projects in this workspace.

> **How to update:** At the end of each project, add a dated bullet under the relevant category. Keep entries brief and actionable.

### HAL / CubeMX
- `FLASH_LATENCY_5` is in `stm32h5xx_hal_flash.h` — must enable `HAL_FLASH_MODULE_ENABLED` in conf even if not directly using flash API.
- `__HAL_RCC_SYSCFG_CLK_ENABLE` / `__HAL_RCC_PWR_CLK_ENABLE` do not exist in STM32H5 — remove from `HAL_MspInit`.
- `--specs=nano.specs` appears in toolchain file AND target link options → linker error. Specify only in toolchain, not in `target_link_options`.
- ST HAL sources have `-Wunused-parameter` warnings. Suppress with `set_source_files_properties` on HAL glob — never add `-Wno-*` globally.
- **CRITICAL — TIM PWM MspInit never called if HAL_TIM_Base_Init is called first.** `HAL_TIM_PWM_Init` only calls `HAL_TIM_PWM_MspInit` when handle state is `RESET`. Calling `HAL_TIM_Base_Init` first sets state to `READY`, so `MspInit` is skipped — GPIO AF and peripheral clock are never configured. Rule: for PWM use, call `HAL_TIM_PWM_Init` only, never pair it with `HAL_TIM_Base_Init`.
- Do not call `MX_ICACHE_Init` before verifying it is needed — it is not required for basic peripheral operation and adds an unnecessary failure point.

### ThreadX / RTOS
*(empty — add lessons here)*

### DMA
*(empty — add lessons here)*

### CAN-FD
*(empty — add lessons here)*

### Debugging / Tooling
- **Serial trace via Python pyserial:** `python -c "import serial; ser=serial.Serial('COM7',115200,timeout=2); ..."` can capture UART output directly in the CLI session, avoiding the need for a separate terminal tool.

### UART
- **UART7 on PE7/PE8 uses `GPIO_AF7_UART7`** (AF7 row in STM32H573 datasheet; AF11 is for other pins such as PA8/PB4).
- For interrupt-driven RX, use `HAL_UART_Receive_IT(&huart, &byte, 1)` and re-arm it at the end of every `HAL_UART_RxCpltCallback`. Do not call blocking `HAL_UART_Transmit` inside the callback — set a flag and transmit from the main loop.

### Mistakes to avoid
- **`nano.specs` + float math — two separate issues:** (1) `__builtin_sqrtf` at `-O0` does NOT inline as VSQRT.F32; GCC falls back to a library call to `sqrtf`. Fix: add `-lm` explicitly via `target_link_libraries(... -lm)`. (2) `sqrtf` in `libm.a` references `__errno` which newlib-nano doesn't provide. Fix: add `int *__errno(void) { static int e=0; return &e; }` stub in `stm32h5xx_it.c`. Both fixes are needed together. Projects using `nosys.specs` (no libc/libm at all) avoid issue #1 because there is no library to fall back to, forcing inline emission.
- **`snprintf` buffer truncation → `-Werror=format-truncation`:** GCC with `-Wall -Werror` detects statically-provable buffer overflows in `snprintf`. Keep buffers large enough for the worst-case format output, or split long messages into multiple calls.
- **CRITICAL — SPI Mode 3 (CPOL=1) + AFCNTR=0 → spurious clock edge on every transfer.** With CPOL=1, SCK idle is HIGH. When `MasterKeepIOState=DISABLE` (AFCNTR=0), SCK goes HiZ when SPE=0 → floats LOW (no pull-up on most boards). Each SPE enable snaps SCK LOW→HIGH = spurious rising edge. The sensor sees an extra clock, shifting all received data by 1 bit. Fix: always set `MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE` (AFCNTR=1) when using SPI Mode 3 (or any CPOL=1 mode).
- **STM32H5 HAL V1.6.0 `HAL_SPI_TransmitReceive` FIFO bug:** The polling 8-bit mode RXWNE handler unconditionally reads 4 bytes from the FIFO even for 2-byte transfers. This leaves orphaned data in the FIFO (SR shows RXWNE=1, RXP=1 after transfer). SPI_CloseTransfer does NOT flush the FIFO. Workaround: use direct-register SPI transfers that properly manage TSIZE, SPE, CSTART, and drain the FIFO.
