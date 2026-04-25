# DirectClaude — TFL_CONNECT_2 Workspace

**MCU:** STM32H573VIT3Q | **HAL:** FW_H5 V1.6.0 | **RTOS:** Azure ThreadX+FileX
**IOC:** `TFL_CONNECT_2_H573.ioc` | **LowLevel:** `LowLevel/` (CubeMX source of truth)

## Two entry points — same behaviour

You can run me from the Claude application **or** from `python run_claude.py`.
Both paths use the **same skill files** under `skills/` and the **same selection
rules**, so quality is identical.

- `skills/**/SKILL.md` — single source of truth (frontmatter `keywords:` and
  `task_types:` drive selection).
- `skill_loader.py` — Python implementation of the selection rule. The
  `run_claude.py` runner calls it before every invocation; the Claude app uses
  the rule below directly.
- `projects/<name>/STATE.md` — compact session cache (auto-load).
- `projects/<name>/CONTEXT.md` — verbose archive (load only if STATE is
  insufficient or the user asks for history).

## Skill selection rule (used by both Python runner and the Claude app)

When you start a task, decide which **task types** are involved:
`arch` (project layout / structure), `code` (firmware logic), `build`
(CMake / linker / warnings), `test` (flash / trace / verify).

Then load skills as follows. **Do NOT read all skills.**

1. Always-load (`priority: base`) when at least one task type applies:
   `embedded-general/coding`, `embedded-general/build`, `embedded-general/testing`.
2. Other skills (`tier2`/`tier3`/`tier4`) load only if **both** are true:
   - one of the skill's `task_types` matches the current task, AND
   - one of the skill's `keywords` appears in the user request.
3. If the user request mentions a project (e.g. "09-sms-modem"), read the
   project's `STATE.md` first. Read `CONTEXT.md` only if STATE is insufficient.
4. If a skill is "PRE-LOADED SKILLS" in the current prompt prefix, **do NOT
   re-read its SKILL.md file** — apply the inlined content directly.

## Autonomous Workflow (one prompt → done)

1. Apply the selection rule above; load only the skills you actually need.
2. If a project is referenced: read its `STATE.md`; reference `projects/04-gsensor/`
   when scaffolding a new project layout.
3. Write ALL files in one pass — do not ask "should I create X?".
4. Build loop: configure → compile → fix every error/warning → repeat until
   the build is clean.
5. Flash: `STM32_Programmer_CLI -l st` first; only flash if a probe is found.
6. Trace verify: open the serial port BEFORE `-hardRst`; iterate until expected
   `[MODULE] ... PASS` lines appear.
7. **Update `STATE.md`** at the end of the session (compact: status, next step,
   any new locked-in fixes). Optionally append a one-paragraph dated entry to
   `CONTEXT.md` for history.

## STATE.md format (template)

Write a STATE.md no longer than ~40 lines. Sections:

```markdown
# <project> — STATE (compact session cache)

## Status
<one or two lines: build clean? flashed? verified?>

## Key facts
<3–8 bullets — only what is non-obvious from CMakeLists or main.c>

## Bug fixes locked in
<bugs that future sessions must not re-introduce>

## Next step
<one or two concrete actions>
```

If you need to record narrative history, append it to `CONTEXT.md` instead.

## Token-saving tips for callers

- `python run_claude.py --continue <project> "next step"` — continuation mode,
  loads STATE.md + a focused skill bundle, leaves CONTEXT.md untouched.
- `python run_claude.py --task code,build "..."` — force the task types.
- `python run_claude.py --debug-skills "..."` — show the selection without
  spending API tokens.

## Projects

| # | Folder | Status |
|---|---|---|
| 01 | `projects/01-blink` | Done ✓ |
| 02 | `projects/02-uart7-echo` | Done ✓ |
| 03 | `projects/03-gsensor` | Done ✓ |
| 04 | `projects/04-gsensor` | Done ✓ (canonical reference) |
| 05 | `projects/05-psram` | Done ✓ |
| 06 | `projects/ThreadX`  | Done ✓ (Azure RTOS blink — tx_timer + dedicated thread) |
| 07 | `projects/07-dual-blink` | Build clean, not yet flashed |
| 09 | `projects/09-sms-modem` | Build clean, flash + trace verify pending |

## External Device Knowledge
`c-embedded/devices/<DatasheetName>.md` — read before writing a driver, update after.
