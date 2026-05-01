"""Confirmation gate — Strategy A (pre-flight classifier).

The bridge inspects every `tool_use` event from the run_claude.py stream and
classifies whether the user must approve before the bridge forwards the event
to the browser. The underlying subprocess has already executed the command by
the time we see it (because run_claude.py uses --dangerously-skip-permissions),
so this gate really controls the UI flow + post-hoc continuation, not the side
effect itself. See ARCHITECTURE.md §6.1.
"""
from __future__ import annotations

import re
from dataclasses import dataclass

# Categories of risky tool calls.
CAT_FLASH = "flash"
CAT_DESTRUCTIVE = "destructive"
CAT_SHELL = "shell"
CAT_PLAN = "plan"
CAT_COST = "cost"

# Bash commands that are always considered safe and never gated.
_SAFE_BASH_PREFIXES = (
    "cmake", "ninja", "ctest", "make",
    "git", "ls", "dir", "pwd", "cd",
    "echo", "cat", "head", "tail", "wc", "find", "grep", "rg",
    "python -m pytest", "pytest",
    "python tools/trace.py", "python -c \"import serial",
    "stm32_programmer_cli -l",  # detection-only is safe
    "stm32_programmer_cli.exe -l",
    "arm-none-eabi-size", "arm-none-eabi-objdump", "arm-none-eabi-readelf",
    "node", "npm run", "yarn", "pnpm",
)

# Bash patterns that are unambiguously risky.
_FLASH_RE = re.compile(
    r"(stm32_programmer_cli(?:\.exe)?\s+(?:[^|]*?)\s-w\b)"
    r"|(openocd[^|]*\bprogram\b)"
    r"|(pyocd\s+flash)",
    re.IGNORECASE,
)
_DESTRUCTIVE_BASH_RE = re.compile(
    r"\brm\s+-rf\b"
    r"|\brm\s+-fr\b"
    r"|\brmdir\s+/s\b"
    r"|\bdel\s+/[sf]\b"
    r"|\bgit\s+(?:reset\s+--hard|push\s+--force|push\s+-f|clean\s+-f)\b"
    r"|\bdd\s+if=",
    re.IGNORECASE,
)

# Edit / Write targets considered destructive.
_DESTRUCTIVE_PATH_RE = re.compile(
    r"(?:^|[\\/])(STATE\.md|CONTEXT\.md|CLAUDE\.md|run_claude\.py|skill_loader\.py)$",
    re.IGNORECASE,
)


@dataclass
class Decision:
    needs_confirm: bool
    category: str | None
    title: str
    detail: str
    command: str  # the literal command/path being gated


def _bash_is_safe(cmd: str) -> bool:
    head = cmd.lstrip().lower()
    return any(head.startswith(p) for p in _SAFE_BASH_PREFIXES)


def classify(tool_name: str, tool_input: dict) -> Decision:
    """Inspect a tool_use event and return whether the bridge should gate it."""
    # ── Bash ──────────────────────────────────────────────────────────────
    if tool_name == "Bash":
        cmd = (tool_input or {}).get("command", "") or ""
        if _FLASH_RE.search(cmd):
            return Decision(
                needs_confirm=True,
                category=CAT_FLASH,
                title="Flash to hardware?",
                detail="The agent will write firmware to the connected MCU. "
                       "Approve to allow the bridge to forward the event and "
                       "continue the run.",
                command=cmd,
            )
        if _DESTRUCTIVE_BASH_RE.search(cmd):
            return Decision(
                needs_confirm=True,
                category=CAT_DESTRUCTIVE,
                title="Destructive shell command?",
                detail="The agent issued a command that may delete files or "
                       "rewrite history. Approve only if intentional.",
                command=cmd,
            )
        if not _bash_is_safe(cmd):
            return Decision(
                needs_confirm=True,
                category=CAT_SHELL,
                title="Run shell command?",
                detail="The agent issued a non-allowlisted shell command.",
                command=cmd,
            )
        return Decision(False, None, "", "", cmd)

    # ── Edit / Write / MultiEdit ─────────────────────────────────────────
    if tool_name in ("Edit", "Write", "MultiEdit"):
        path = (tool_input or {}).get("file_path") or (tool_input or {}).get("path") or ""
        if _DESTRUCTIVE_PATH_RE.search(path.replace("\\", "/")):
            return Decision(
                needs_confirm=True,
                category=CAT_DESTRUCTIVE,
                title=f"Modify protected file?",
                detail=f"The agent is about to modify {path}. This file is "
                       f"part of the workspace contract — approve only if "
                       f"intentional.",
                command=path,
            )
        return Decision(False, None, "", "", path)

    # ── Read / Grep / Glob and friends ───────────────────────────────────
    return Decision(False, None, "", "", "")
