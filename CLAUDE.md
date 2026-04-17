# DirectClaude — TFL_CONNECT_2 Workspace

**MCU:** STM32H573VIT3Q | **HAL:** FW_H5 V1.6.0 | **RTOS:** Azure ThreadX+FileX
**IOC:** `TFL_CONNECT_2_H573.ioc` | **LowLevel:** `LowLevel/` (CubeMX source of truth)

## Skill Map — load before acting

| Situation | Skills to read |
|---|---|
| Starting any new project | `skills/embedded-general/architecture/SKILL.md` → `skills/stm32/architecture/SKILL.md` → `skills/project-tfl/architecture/SKILL.md` |
| Writing firmware code | `skills/embedded-general/coding/SKILL.md` → `skills/stm32/coding/SKILL.md` → `skills/project-tfl/coding/SKILL.md` |
| Creating CMake / build files | `skills/stm32/build/SKILL.md` → `skills/project-tfl/build/SKILL.md` |
| Building / fixing compile errors | `skills/embedded-general/build/SKILL.md` → `skills/stm32/build/SKILL.md` |
| Flashing + trace verification | `skills/embedded-general/testing/SKILL.md` → `skills/stm32/testing/SKILL.md` → `skills/project-tfl/testing/SKILL.md` |
| Writing a peripheral driver | `skills/stm32/coding/SKILL.md` + check `c-embedded/devices/<chip>.md` |
| Integrating Azure RTOS ThreadX | `skills/stm32/threadx/SKILL.md` (+ reference `projects/ThreadX/`) |

## Autonomous Workflow (one prompt → done)

1. Read skills per table above
2. If existing project: read its `CONTEXT.md` first; reference `projects/04-gsensor/` for layout
3. Write ALL files in one pass (no asking "should I create X?")
4. Build loop → flash → trace verify → update `CONTEXT.md`
5. Do NOT stop until trace confirms correct behaviour

## Projects

| # | Folder | Status |
|---|---|---|
| 01 | `projects/01-blink` | Done ✓ |
| 02 | `projects/02-uart7-echo` | Done ✓ |
| 03 | `projects/03-gsensor` | Done ✓ |
| 04 | `projects/04-gsensor` | Done ✓ (canonical reference) |
| 05 | `projects/05-psram` | Done ✓ |
| 06 | `projects/ThreadX`  | Done ✓ (Azure RTOS blink — tx_timer + dedicated thread) |

## External Device Knowledge
`c-embedded/devices/<DatasheetName>.md` — read before writing a driver, update after.
