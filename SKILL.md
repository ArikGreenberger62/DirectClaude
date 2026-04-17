---
name: embedded-general/architecture
description: General embedded firmware architecture principles. Read when starting a new embedded project or designing module structure.
---

# Embedded Firmware Architecture

## Module Structure
- One `.c` / `.h` pair per responsibility (driver, subsystem, app layer)
- Keep files under ~300 lines; split by sub-responsibility beyond that
- Headers are self-contained, include-guarded, public contract only (types, constants, prototypes)
- No `extern` variables in headers — expose state via accessor functions
- A reader should understand a module's contract from the header alone

## Layering
```
app_*.c          ← business logic, no direct HAL calls
drivers/*.c      ← thin HAL wrappers, swappable with mocks
Core/Src/        ← main, ISR, init glue
```

## Host-Side Testing
- All business logic in `app_*.c` — zero direct HAL calls
- HAL in thin `drivers/*.c` wrappers excluded from host build
- `tests/` folder with native CMake target (`-DBUILD_TESTS=ON`)
- Use `#ifdef UNIT_TEST` to swap in mocks; run via `ctest`

## SELF_TEST Mode
When compiled with `-DSELF_TEST`:
- Call `SelfTest_Run()` after all `MX_*_Init()`, before main loop
- Lives in `Core/Src/self_test.c` / `Core/Inc/self_test.h`
- Reports over debug UART: `[SELF_TEST] <module>: PASS` or `FAIL - <reason>`
- On failure: blink error LED and halt
- Minimum checks: SYSCLK freq, RAM write/read pattern, IWDG running, UART transmit

## CONTEXT.md — Per-Project Memory
Every project folder must have `CONTEXT.md` with:
```markdown
## Current State
## Architecture Decisions
## Last Session
- What finished / what is broken / what is next
```
Read it first at the start of every session. Update at the end.
