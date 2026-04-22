#!/usr/bin/env python3
"""
run_claude.py — Run Claude Code CLI autonomously against a DirectClaude workspace.

Usage:
    python run_claude.py [OPTIONS] "your prompt here"

Options:
    --folder PATH     Path to the workspace folder (must contain CLAUDE.md).
                      Defaults to the folder containing this script.
    --model MODEL     Claude model alias (e.g. sonnet, opus). Default: sonnet.
    --effort LEVEL    Effort level: low, medium, high, max. Default: max.
    --key-file PATH   File containing the Anthropic API key.
                      Defaults to ApiKeyArik in the workspace folder.
    --no-stream       Buffer output instead of streaming (simpler, less progress).
    -h, --help        Show this help.

Example:
    python run_claude.py "create project 7: RED led blinks 1 pulse/sec 30%% duty,
        GREEN led blinks twice at 50%% duty while RED is off"
"""

import argparse
import json
import os
import subprocess
import sys
import textwrap
from pathlib import Path

# ── ANSI colours ──────────────────────────────────────────────────────────────
RESET  = "\033[0m"
BOLD   = "\033[1m"
CYAN   = "\033[96m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
RED    = "\033[91m"
DIM    = "\033[2m"
MAGENTA= "\033[95m"

def color(text: str, *codes: str) -> str:
    return "".join(codes) + text + RESET


# ── Stream-JSON parser ─────────────────────────────────────────────────────────
def handle_stream_event(event: dict) -> None:
    """Pretty-print a single stream-json event from Claude Code."""
    etype = event.get("type", "")

    if etype == "system":
        subtype = event.get("subtype", "")
        if subtype == "init":
            model = event.get("model", "unknown")
            print(color(f"\n◆ Session started  [model: {model}]", BOLD, CYAN))
            print(color("─" * 60, DIM))

    elif etype == "assistant":
        message = event.get("message", {})
        for block in message.get("content", []):
            btype = block.get("type", "")
            if btype == "text":
                text = block.get("text", "")
                if text.strip():
                    print(color(text, RESET))
            elif btype == "tool_use":
                tool  = block.get("name", "?")
                inp   = block.get("input", {})
                desc  = _summarise_tool(tool, inp)
                print(color(f"\n  ▶ {tool}  {desc}", YELLOW))

    elif etype == "tool_result":
        content = event.get("content", "")
        if isinstance(content, list):
            for part in content:
                if part.get("type") == "text":
                    snippet = part.get("text", "")[:200].strip()
                    if snippet:
                        print(color(f"    └─ {snippet}", DIM))
        elif isinstance(content, str):
            snippet = content[:200].strip()
            if snippet:
                print(color(f"    └─ {snippet}", DIM))

    elif etype == "result":
        subtype = event.get("subtype", "")
        cost    = event.get("cost_usd")
        turns   = event.get("num_turns", "?")
        print(color("\n─" * 60, DIM))
        if subtype == "success":
            result_text = event.get("result", "")
            if result_text:
                print(color(result_text, RESET))
            cost_str = f"  cost: ${cost:.4f}" if cost else ""
            print(color(f"\n✔ Done  [{turns} turns{cost_str}]", BOLD, GREEN))
        else:
            reason = event.get("reason", subtype)
            print(color(f"\n✘ Stopped: {reason}", BOLD, RED))

    elif etype == "error":
        msg = event.get("message", str(event))
        print(color(f"\n✘ Error: {msg}", BOLD, RED))


def _summarise_tool(tool: str, inp: dict) -> str:
    """One-line summary of a tool call for the progress display."""
    if tool in ("Write", "Edit", "MultiEdit"):
        return inp.get("file_path") or inp.get("path") or ""
    if tool == "Bash":
        cmd = inp.get("command", "")
        return cmd[:80] + ("…" if len(cmd) > 80 else "")
    if tool in ("Read", "View"):
        return inp.get("file_path") or inp.get("path") or ""
    if tool == "Grep":
        return f'"{inp.get("pattern","")}"'
    if tool == "Glob":
        return inp.get("pattern", "")
    return str(inp)[:80]


# ── Main ───────────────────────────────────────────────────────────────────────
def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Claude Code CLI autonomously in a DirectClaude workspace.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent(__doc__ or ""),
    )
    parser.add_argument("prompt", nargs="+", help="Prompt to send to Claude Code")
    parser.add_argument(
        "--folder", "-f",
        default=None,
        help="Workspace folder containing CLAUDE.md (default: script directory)",
    )
    parser.add_argument("--model",  "-m", default="sonnet",  help="Model alias (default: sonnet)")
    parser.add_argument("--effort", "-e", default="max",     help="Effort level (default: max)")
    parser.add_argument("--key-file", "-k", default=None,    help="File containing Anthropic API key")
    parser.add_argument("--no-stream", action="store_true",  help="Buffer instead of streaming")
    args = parser.parse_args()

    # ── Resolve workspace folder ───────────────────────────────────────────────
    script_dir = Path(__file__).resolve().parent
    folder = Path(args.folder).resolve() if args.folder else script_dir

    claude_md = folder / "CLAUDE.md"
    if not claude_md.exists():
        print(color(f"✘ CLAUDE.md not found in: {folder}", BOLD, RED))
        print(  "  Pass a different --folder or run from the workspace root.")
        return 1

    # ── Resolve API key ────────────────────────────────────────────────────────
    env = os.environ.copy()
    if "ANTHROPIC_API_KEY" not in env:
        key_file = Path(args.key_file) if args.key_file else folder / "ApiKeyArik"
        if key_file.exists():
            api_key = key_file.read_text(encoding="utf-8").strip()
            env["ANTHROPIC_API_KEY"] = api_key
            print(color(f"◆ API key loaded from: {key_file.name}", DIM))
        else:
            print(color("✘ No ANTHROPIC_API_KEY set and no key file found.", BOLD, RED))
            print( "  Set ANTHROPIC_API_KEY env var or provide --key-file.")
            return 1

    # ── Assemble prompt ────────────────────────────────────────────────────────
    user_request = " ".join(args.prompt)

    # Explicitly enforce the full Autonomous Workflow from CLAUDE.md so that
    # every stage (build → flash → trace verify → CONTEXT.md update) runs even
    # when invoked non-interactively via this script.
    prompt_text = f"""\
Follow the Autonomous Workflow defined in CLAUDE.md exactly:

1. Read the required skills from the Skill Map for this task (architecture, coding, build, testing).
2. If the project folder already exists, read its CONTEXT.md first.
3. Write ALL required files in one pass — do NOT ask for confirmation.
4. Build loop: compile, fix all errors, repeat until the build is clean.
5. Flash the firmware to the hardware using the correct flash tool/script.
6. Verify correct behaviour via serial trace (UART/SWO); do NOT stop until trace confirms success.
7. Update (or create) the project CONTEXT.md with the final status.

Do NOT stop after the build step. Steps 5 (flash) and 6 (trace verify) are mandatory.

User request: {user_request}"""

    # ── Build claude command ───────────────────────────────────────────────────
    output_fmt = "text" if args.no_stream else "stream-json"
    cmd = [
        "claude",
        "--print",                         # non-interactive
        "--dangerously-skip-permissions",  # no confirmation prompts
        "--model",  args.model,
        "--effort", args.effort,
        "--output-format", output_fmt,
    ]
    if not args.no_stream:
        cmd.append("--verbose")            # required for stream-json
    cmd.append(prompt_text)

    print(color(f"\n◆ Workspace : {folder}", BOLD))
    print(color(f"◆ Model     : {args.model}  effort={args.effort}", BOLD))
    print(color(f"◆ Prompt    : {user_request[:120]}{'…' if len(user_request)>120 else ''}", BOLD))
    print(color("─" * 60, DIM))

    # ── Run ────────────────────────────────────────────────────────────────────
    if args.no_stream:
        # Simple: capture all output, print at end
        result = subprocess.run(
            cmd,
            cwd=str(folder),
            env=env,
            capture_output=False,
            text=True,
        )
        return result.returncode

    # Streaming: parse NDJSON events line by line
    proc = subprocess.Popen(
        cmd,
        cwd=str(folder),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,          # line-buffered
    )

    # Read stderr in a simple way alongside stdout
    import threading

    def drain_stderr(pipe):
        for line in pipe:
            line = line.rstrip()
            if line:
                print(color(f"[stderr] {line}", MAGENTA), file=sys.stderr)

    stderr_thread = threading.Thread(target=drain_stderr, args=(proc.stderr,), daemon=True)
    stderr_thread.start()

    for raw_line in proc.stdout:
        raw_line = raw_line.rstrip()
        if not raw_line:
            continue
        try:
            event = json.loads(raw_line)
            handle_stream_event(event)
        except json.JSONDecodeError:
            # Plain text line (shouldn't happen with stream-json, but just in case)
            print(raw_line)

    proc.wait()
    stderr_thread.join(timeout=2)

    if proc.returncode != 0:
        print(color(f"\n✘ claude exited with code {proc.returncode}", BOLD, RED))
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
