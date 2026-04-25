#!/usr/bin/env python3
"""
run_claude.py — Run Claude Code CLI autonomously against a DirectClaude workspace.

Behaviour parity with the Claude application:
  * Same SKILL.md files as the Claude app (read by `skill_loader.py`).
  * The selected skills are pre-bundled into the prompt prefix so Claude
    does NOT need to re-discover or re-read them — saving tokens AND
    making behaviour deterministic across both entry points.
  * If --continue / --project is given, the project's `STATE.md` (compact
    session cache) is bundled too. The full `CONTEXT.md` is loaded only on
    explicit --include-context.

Token-saving flags:
  --task TASK[,TASK]   Force task types (arch,code,build,test). Default: auto.
  --no-skills          Skip skill bundling entirely.
  --debug-skills       Print which skills were selected, then exit.
  --continue PROJECT   Inject projects/PROJECT/STATE.md into the prompt.
  --project PROJECT    Same as --continue, but does not flip the autonomous
                       prompt to "continuation" wording.
  --include-context    Also load full CONTEXT.md (verbose, opt-in).
  --no-autonomous      Skip the autonomous-workflow preamble.

Usage examples:
    python run_claude.py "fix the SPI prescaler bug in 04-gsensor"
    python run_claude.py --continue 09-sms-modem "flash and verify trace"
    python run_claude.py --task code "implement TCP socket open in modem driver"
    python run_claude.py --debug-skills "create a new ThreadX project"
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import textwrap
import threading
from pathlib import Path

import skill_loader as sl

# ── Console: force UTF-8 so unicode bullets render on Windows ────────────────
try:
    sys.stdout.reconfigure(encoding="utf-8")  # type: ignore[attr-defined]
    sys.stderr.reconfigure(encoding="utf-8")  # type: ignore[attr-defined]
except (AttributeError, OSError):
    pass

# ── ANSI colours ──────────────────────────────────────────────────────────────
RESET   = "\033[0m"
BOLD    = "\033[1m"
CYAN    = "\033[96m"
GREEN   = "\033[92m"
YELLOW  = "\033[93m"
RED     = "\033[91m"
DIM     = "\033[2m"
MAGENTA = "\033[95m"


def color(text: str, *codes: str) -> str:
    return "".join(codes) + text + RESET


# ── Stream-JSON parser ─────────────────────────────────────────────────────────
def handle_stream_event(event: dict) -> None:
    etype = event.get("type", "")
    if etype == "system" and event.get("subtype", "") == "init":
        model = event.get("model", "unknown")
        print(color(f"\n◆ Session started  [model: {model}]", BOLD, CYAN))
        print(color("─" * 60, DIM))
    elif etype == "assistant":
        for block in event.get("message", {}).get("content", []):
            btype = block.get("type", "")
            if btype == "text":
                text = block.get("text", "")
                if text.strip():
                    print(color(text, RESET))
            elif btype == "tool_use":
                tool = block.get("name", "?")
                desc = _summarise_tool(tool, block.get("input", {}))
                print(color(f"\n  ▶ {tool}  {desc}", YELLOW))
    elif etype == "tool_result":
        content = event.get("content", "")
        if isinstance(content, list):
            for part in content:
                if part.get("type") == "text":
                    snip = part.get("text", "")[:200].strip()
                    if snip:
                        print(color(f"    └─ {snip}", DIM))
        elif isinstance(content, str):
            snip = content[:200].strip()
            if snip:
                print(color(f"    └─ {snip}", DIM))
    elif etype == "result":
        subtype = event.get("subtype", "")
        cost = event.get("cost_usd")
        turns = event.get("num_turns", "?")
        print(color("\n─" * 60, DIM))
        if subtype == "success":
            result_text = event.get("result", "")
            if result_text:
                print(color(result_text, RESET))
            cost_str = f"  cost: ${cost:.4f}" if cost else ""
            print(color(f"\n✔ Done  [{turns} turns{cost_str}]", BOLD, GREEN))
        else:
            print(color(f"\n✘ Stopped: {event.get('reason', subtype)}", BOLD, RED))
    elif etype == "error":
        print(color(f"\n✘ Error: {event.get('message', str(event))}", BOLD, RED))


def _summarise_tool(tool: str, inp: dict) -> str:
    if tool in ("Write", "Edit", "MultiEdit"):
        return inp.get("file_path") or inp.get("path") or ""
    if tool == "Bash":
        cmd = inp.get("command", "")
        return cmd[:80] + ("…" if len(cmd) > 80 else "")
    if tool in ("Read", "View"):
        return inp.get("file_path") or inp.get("path") or ""
    if tool == "Grep":
        return f'"{inp.get("pattern", "")}"'
    if tool == "Glob":
        return inp.get("pattern", "")
    return str(inp)[:80]


# ── Prompt assembly ───────────────────────────────────────────────────────────
# Shared closed-loop fix-and-retry block. Both preambles include this so the
# runner never stops at the first failure — it iterates build/flash/trace
# until the trace confirms the requested behaviour, OR until the iteration
# cap (passed via `max_iter`) is reached.
def _fix_loop_block(max_iter: int) -> str:
    return textwrap.dedent(f"""\
        ## Closed-loop fix-and-retry policy (mandatory)

        The session is NOT done until the trace confirms the requested
        behaviour. Apply this loop on every step that can fail:

          BUILD:  configure → compile → if any error/warning → fix the root
                  cause in source/CMake → rebuild → repeat.
                  HARD CAP: at most {max_iter} fix-rebuild cycles.

          FLASH:  `STM32_Programmer_CLI -l st` → if no probe found, STOP and
                  report (do not flash blindly). If probe found:
                  `STM32_Programmer_CLI -c port=SWD freq=4000 -w <elf> -rst`.
                  If flash fails → diagnose (locked? wrong .elf path? voltage?)
                  → fix → retry. HARD CAP: 3 flash retries.

          TRACE:  open `COM7` @ 115200 BEFORE issuing `-hardRst` (else startup
                  messages are lost). Read for ≥10 s. Compare against the
                  expected `[MODULE] ... PASS` / runtime tag lines.
                  If mismatch:
                    1. Identify the FIRST wrong/missing line in the trace.
                    2. Diagnose from registers / `[REG]` dumps / module log.
                    3. Edit source → rebuild → reflash → re-read trace.
                    4. Repeat.
                  HARD CAP: at most {max_iter} diagnose-edit-reflash cycles.
                  Do NOT declare success on a "build clean" alone.

        ## Failure exit (when a hard cap is reached)

        If any hard cap is hit, STOP iterating and write a structured
        failure record so the NEXT session can resume:

          1. Append/overwrite a `## FAILURE` section in the project's
             `STATE.md`, including:
                - which step capped (BUILD / FLASH / TRACE)
                - the last error message or last trace dump (≤30 lines)
                - the most recent hypothesis you had not yet tried
                - the iteration count consumed
          2. Update `## Next step` in STATE.md to start with the word
             "RESUME:" so the next run knows it is picking up after a
             capped failure.
          3. Print `[RESULT] CAPPED — see STATE.md FAILURE section`
             to the trace UART (if MCU still alive) and to stdout.
          4. Exit cleanly. Do not keep retrying.

        Other hard stops (also exit-clean): no ST-LINK probe present;
        user prompt explicitly says "build only" or "do not flash";
        an unrecoverable hardware issue.
        """)

_AUTONOMOUS_PREAMBLE_TEXT = textwrap.dedent("""\
    Follow the Autonomous Workflow defined in CLAUDE.md exactly:

    1. Use the PRE-LOADED SKILLS below — do NOT re-read SKILL.md files.
       If a needed skill was not pre-loaded, you may read it once.
    2. If a project folder is referenced and STATE.md is supplied below,
       trust it as the session cache; only read CONTEXT.md if STATE is
       insufficient.
    3. Write ALL required files in one pass — do not ask for confirmation.
    4. Build → Flash → Trace verify (apply the closed-loop policy below).
    5. Update the project STATE.md (compact) at the end of the session.
       Optionally append a one-paragraph entry to CONTEXT.md.
    """)

_CONTINUATION_PREAMBLE_TEXT = textwrap.dedent("""\
    This is a CONTINUATION session on an existing project. The project's
    STATE.md (compact last-session cache) is included below. Use it as
    your starting point — do NOT re-read the full CONTEXT.md unless
    STATE is insufficient for the current task.

    1. Apply the PRE-LOADED SKILLS without re-reading their source files.
    2. Read STATE.md "Next step" and "Status" — that is where you start.
       If "Next step" begins with "RESUME:", a previous session capped on
       a failure — read the `## FAILURE` section and continue from the
       last untried hypothesis.
    3. The user request below is either:
       (a) a new feature on top of the current state, OR
       (b) a problem report against the current state.
       In BOTH cases, apply the closed-loop policy below until the trace
       confirms the new behaviour or the bug is fixed (or the cap hits).
    4. After the work, REWRITE STATE.md to reflect the new state and the
       new "Next step". Append a dated one-paragraph entry to CONTEXT.md.
    """)


def autonomous_preamble(max_iter: int) -> str:
    return _AUTONOMOUS_PREAMBLE_TEXT + "\n" + _fix_loop_block(max_iter)


def continuation_preamble(max_iter: int) -> str:
    return _CONTINUATION_PREAMBLE_TEXT + "\n" + _fix_loop_block(max_iter)


_PROJECT_DIR_RE = re.compile(r"(?:^|[\s/`'\"])((?:\d{2}-[a-z][a-z0-9_-]*)|ThreadX)\b",
                              re.IGNORECASE)


def autodetect_project(workspace: Path, prompt: str) -> str | None:
    """Find a `NN-name` or `ThreadX` token in the prompt that maps to an
    existing project folder. Returns the folder name, or None."""
    projects_dir = workspace / "projects"
    if not projects_dir.exists():
        return None
    existing = {p.name.lower(): p.name for p in projects_dir.iterdir() if p.is_dir()}
    for m in _PROJECT_DIR_RE.finditer(prompt):
        token = m.group(1)
        hit = existing.get(token.lower())
        if hit:
            return hit
    return None


def parse_task_types(s: str | None) -> set[str] | None:
    if not s:
        return None
    out = {t.strip().lower() for t in s.split(",") if t.strip()}
    valid = {"arch", "code", "build", "test"}
    bad = out - valid
    if bad:
        raise SystemExit(f"unknown --task value(s): {sorted(bad)} "
                         f"(allowed: {sorted(valid)})")
    return out


def build_prompt(args, workspace: Path, user_request: str) -> tuple[str, list, str | None, bool]:
    """Return (full_prompt, selected_skills, project_used, auto_detected)."""
    parts: list[str] = []

    # Auto-detect project when neither --continue nor --project was supplied.
    auto_detected = False
    project = args.continue_project or args.project
    if not project and not args.no_autodetect:
        hit = autodetect_project(workspace, user_request)
        if hit:
            project = hit
            auto_detected = True

    # Autonomous preamble (stable, cache-friendly).
    # Continuation preamble fires whenever a project is loaded — explicit OR
    # auto-detected — because the user's request is necessarily an addition
    # to / fix of that project's existing state.
    use_continuation = bool(args.continue_project) or auto_detected
    if not args.no_autonomous:
        parts.append(continuation_preamble(args.max_iterations) if use_continuation
                     else autonomous_preamble(args.max_iterations))

    selected: list = []
    project_state: str | None = None

    # Skill bundle.
    if not args.no_skills:
        skills = sl.discover_skills(workspace / "skills")
        forced = parse_task_types(args.task)
        selected = sl.select_skills(skills, user_request, task_types=forced)

    # Project state (STATE.md compact cache; CONTEXT.md optional).
    if project:
        proj_dir = workspace / "projects" / project
        if not proj_dir.exists():
            raise SystemExit(f"project folder not found: {proj_dir}")
        state_md = proj_dir / "STATE.md"
        if state_md.exists():
            project_state = state_md.read_text(encoding="utf-8")
        elif (proj_dir / "CONTEXT.md").exists():
            print(color(f"⚠ STATE.md missing in {project}/, falling back to CONTEXT.md", YELLOW))
            project_state = (proj_dir / "CONTEXT.md").read_text(encoding="utf-8")
        if args.include_context and (proj_dir / "CONTEXT.md").exists():
            ctx = (proj_dir / "CONTEXT.md").read_text(encoding="utf-8")
            project_state = (project_state or "") + "\n\n## FULL CONTEXT (verbose)\n" + ctx

    if not args.no_skills:
        parts.append(sl.assemble_bundle(selected, project_state=project_state))
    elif project_state:
        parts.append("## PROJECT STATE\n" + project_state)

    parts.append(f"## USER REQUEST\n{user_request}\n")

    return "\n".join(parts), selected, project, auto_detected


# ── Main ──────────────────────────────────────────────────────────────────────
def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Claude Code CLI autonomously in a DirectClaude workspace.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent(__doc__ or ""),
    )
    parser.add_argument("prompt", nargs="+", help="Prompt to send to Claude Code")
    parser.add_argument("--folder", "-f", default=None,
                        help="Workspace folder (default: script directory)")
    parser.add_argument("--model", "-m", default="sonnet")
    parser.add_argument("--effort", "-e", default="max")
    parser.add_argument("--key-file", "-k", default=None)
    parser.add_argument("--no-stream", action="store_true")

    # Skill loader controls
    parser.add_argument("--task", default=None,
                        help="comma-separated task types: arch,code,build,test")
    parser.add_argument("--no-skills", action="store_true",
                        help="skip skill bundling (smaller prompt; use for trivial Q&A)")
    parser.add_argument("--debug-skills", action="store_true",
                        help="print selection + bundle size and exit (no API call)")

    # Project state controls
    parser.add_argument("--continue", "-c", dest="continue_project", default=None,
                        metavar="PROJECT",
                        help="continue work on a project (loads STATE.md, "
                             "uses continuation preamble)")
    parser.add_argument("--project", "-p", default=None,
                        help="reference a project (loads STATE.md, keeps autonomous preamble)")
    parser.add_argument("--include-context", action="store_true",
                        help="also load full CONTEXT.md alongside STATE.md")
    parser.add_argument("--no-autodetect", action="store_true",
                        help="disable automatic project detection from prompt text")
    parser.add_argument("--max-iterations", "-n", type=int, default=10,
                        metavar="N",
                        help="hard cap on build / trace fix-and-retry cycles "
                             "(default: 10). On cap, Claude writes a FAILURE "
                             "section to STATE.md and exits cleanly.")
    parser.add_argument("--no-autonomous", action="store_true",
                        help="skip the autonomous-workflow preamble (free-form session)")
    parser.add_argument("--show-prompt", action="store_true",
                        help="print final prompt instead of invoking claude")

    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    folder = Path(args.folder).resolve() if args.folder else script_dir

    claude_md = folder / "CLAUDE.md"
    if not claude_md.exists():
        print(color(f"✘ CLAUDE.md not found in: {folder}", BOLD, RED))
        return 1

    # API key (skipped for debug-only modes).
    env = os.environ.copy()
    needs_key = not (args.debug_skills or args.show_prompt)
    if needs_key and "ANTHROPIC_API_KEY" not in env:
        key_file = Path(args.key_file) if args.key_file else folder / "ApiKeyArik"
        if key_file.exists():
            env["ANTHROPIC_API_KEY"] = key_file.read_text(encoding="utf-8").strip()
            print(color(f"◆ API key loaded from: {key_file.name}", DIM))
        else:
            print(color("✘ No ANTHROPIC_API_KEY and no key file found.", BOLD, RED))
            return 1

    user_request = " ".join(args.prompt)
    full_prompt, selected, project_used, auto_detected = build_prompt(
        args, folder, user_request)

    # ── Header / debug ────────────────────────────────────────────────────────
    print(color(f"\n◆ Workspace : {folder}", BOLD))
    print(color(f"◆ Model     : {args.model}  effort={args.effort}", BOLD))
    if args.continue_project:
        print(color(f"◆ Continue  : {args.continue_project}", BOLD))
    elif auto_detected and project_used:
        print(color(f"◆ Continue  : {project_used}  (auto-detected from prompt)", BOLD, CYAN))
    elif args.project:
        print(color(f"◆ Project   : {args.project}", BOLD))
    if selected:
        names = ", ".join(s.name for s in selected)
        print(color(f"◆ Skills    : {len(selected)}  [{names}]", BOLD))
    elif not args.no_skills:
        print(color("◆ Skills    : (none triggered)", BOLD))
    else:
        print(color("◆ Skills    : DISABLED (--no-skills)", BOLD))
    if not args.no_autonomous:
        print(color(f"◆ Max iters : {args.max_iterations}  (build & trace fix-loop cap)", BOLD))
    tok = sl.rough_tokens(full_prompt)
    print(color(f"◆ Prefix    : ~{tok} tokens  ({len(full_prompt)} chars)", BOLD))
    print(color(f"◆ Prompt    : {user_request[:120]}{'…' if len(user_request) > 120 else ''}", BOLD))
    print(color("─" * 60, DIM))

    if args.debug_skills:
        for s in selected:
            print(f"  [{s.priority:5s}] {s.name}")
        return 0
    if args.show_prompt:
        print(full_prompt)
        return 0

    # ── Invoke claude CLI ────────────────────────────────────────────────────
    output_fmt = "text" if args.no_stream else "stream-json"
    cmd = [
        "claude",
        "--print",
        "--dangerously-skip-permissions",
        "--model", args.model,
        "--effort", args.effort,
        "--output-format", output_fmt,
    ]
    if not args.no_stream:
        cmd.append("--verbose")
    cmd.append(full_prompt)

    if args.no_stream:
        return subprocess.run(cmd, cwd=str(folder), env=env, text=True).returncode

    proc = subprocess.Popen(
        cmd, cwd=str(folder), env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, encoding="utf-8", errors="replace", bufsize=1,
    )

    def drain_stderr(pipe):
        for line in pipe:
            line = line.rstrip()
            if line:
                print(color(f"[stderr] {line}", MAGENTA), file=sys.stderr)

    stderr_thread = threading.Thread(target=drain_stderr, args=(proc.stderr,), daemon=True)
    stderr_thread.start()

    for raw in proc.stdout:
        raw = raw.rstrip()
        if not raw:
            continue
        try:
            handle_stream_event(json.loads(raw))
        except json.JSONDecodeError:
            print(raw)

    proc.wait()
    stderr_thread.join(timeout=2)
    if proc.returncode != 0:
        print(color(f"\n✘ claude exited with code {proc.returncode}", BOLD, RED))
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
