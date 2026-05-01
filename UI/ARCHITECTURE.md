# CCoreAi POC — Local Bridge Architecture

> **Brief for Claude Code (or any implementer):** wire the browser-based CCoreAi POC UI to the existing `run_claude.py` script **without modifying the Python script itself**. This document specifies the architecture, contracts, and acceptance criteria.

---

## 1. Constraints (non-negotiable)

1. **Do NOT modify `run_claude.py`** — the tool is owned by the firmware team and must stay reproducible from the CLI.
2. The web UI is a **local-only** SPA served from `127.0.0.1` — no external services, no auth servers.
3. Bridge is a thin process wrapper. It captures stdout/stderr and streams it to the browser; it never edits prompts or interprets stream content semantically.
4. Confirmation gating must work end-to-end: when a "risky" tool call is detected, the run pauses and only resumes after the user clicks **Approve** in the browser.

---

## 2. High-level architecture

```
┌──────────────────────────┐         ┌──────────────────────────────┐
│  Browser  (CCoreAi.html) │         │  CCoreBridge  (Python)       │
│                          │ ◄──WS──►│                              │
│  - Login / Projects /    │   8765  │  FastAPI + uvicorn           │
│    Workspace UI          │         │  ├─ /static  (serves SPA)    │
│  - WebSocket client      │         │  ├─ /api/projects            │
│  - Speaks JSON events    │         │  ├─ /api/run     (POST)      │
│  - Sends approve/reject  │         │  └─ /ws/run/<run_id>         │
└──────────────────────────┘         │                              │
                                     │  Per-run subprocess:         │
                                     │  ┌────────────────────────┐  │
                                     │  │ python run_claude.py … │  │
                                     │  │  (UNMODIFIED)          │  │
                                     │  └─────────┬──────────────┘  │
                                     │            │ stdout (stream- │
                                     │            │ json) + stderr  │
                                     │            ▼                 │
                                     │  Stream parser → enrich →    │
                                     │  push events to WS clients   │
                                     └──────────────────────────────┘
                                                ▲
                                                │ (file IO)
                                     ~/DirectClaude/ workspace
                                       ├─ run_claude.py
                                       ├─ CLAUDE.md, SKILL.md
                                       ├─ projects/<n>/STATE.md, …
                                       └─ skills/, c-embedded/, …
```

**One process, one runner.** The user can have at most one active run per workspace; concurrent runs queue. This matches `run_claude.py`'s assumption that it owns the workspace.

---

## 3. Components

### 3.1 CCoreBridge (Python service)

A small FastAPI app that the user starts once with `ccorebridge ~/DirectClaude` (or a `.bat` shim on Windows). Recommended layout:

```
ccorebridge/
├── pyproject.toml
├── ccorebridge/
│   ├── __main__.py         # CLI entrypoint  (uvicorn launcher)
│   ├── server.py           # FastAPI routes
│   ├── runner.py           # Subprocess orchestration + stream parsing
│   ├── confirm.py          # Confirmation gate logic
│   ├── projects.py         # Reads workspace folder for project list
│   └── static/             # Built CCoreAi.html + assets
└── README.md
```

**Why FastAPI?** Native WebSocket + async subprocess support, single-binary packaging via `pyinstaller` later.

### 3.2 Browser SPA

The HTML/JSX in this repo. Single page, three routes (login → projects → workspace) all client-side. The login page authenticates against the bridge using a token written to a file (`~/.ccorebridge/token`) on first launch — it is **not** cloud auth.

---

## 4. HTTP / WebSocket contract

### 4.1 REST

| Method | Path                        | Purpose                                      |
|--------|-----------------------------|----------------------------------------------|
| GET    | `/api/health`               | `{ok, version, workspace}` heartbeat         |
| POST   | `/api/auth`                 | `{token}` → `{session}` (local token check)  |
| GET    | `/api/projects`             | List `projects/*` folders + STATE.md summary |
| GET    | `/api/projects/{id}`        | STATE.md, file count, last run timestamp     |
| POST   | `/api/run`                  | Start a run, returns `{run_id}`              |
| POST   | `/api/run/{id}/confirm`     | `{action: "approve" \| "reject", patch?}`    |
| POST   | `/api/run/{id}/stop`        | SIGTERM the subprocess                       |
| GET    | `/api/run/{id}/log`         | Full transcript (for reload after refresh)   |

**POST `/api/run` body:**
```json
{
  "project": "09-sms-modem",        // optional, becomes --continue
  "prompt": "flash and verify…",
  "task": "code,build,test",        // optional
  "max_iterations": 10,
  "model": "sonnet",
  "effort": "max"
}
```

The bridge translates this to:
```
python run_claude.py --continue 09-sms-modem --task code,build,test \
                     --max-iterations 10 --model sonnet --effort max \
                     "flash and verify…"
```

### 4.2 WebSocket (`/ws/run/<run_id>`)

**Server → Browser** events (line-delimited JSON):

```jsonc
// Run lifecycle
{"type":"run.start",   "run_id":"r_…", "started_at":"2026-04-28T14:02:11Z"}
{"type":"run.end",     "exit_code":0,  "cost_usd":0.184, "turns":11}

// Stream events from run_claude.py (passed through, lightly enriched with timestamp)
{"type":"session",  "model":"…",        "ts":"…"}
{"type":"text",     "text":"…",         "ts":"…"}
{"type":"tool",     "name":"Bash",
                    "input":"…",
                    "ts":"…",
                    "tool_use_id":"toolu_…",
                    "needs_confirm":true,
                    "category":"flash"}     // ← bridge-added classification
{"type":"result",   "text":"…", "tool_use_id":"toolu_…", "ts":"…"}
{"type":"meta",     "key":"skills", "value":[…]}
{"type":"error",    "message":"…"}

// Bridge-emitted gate
{"type":"confirm.request",
 "id":"cf_…",
 "category":"flash",                 // flash|destructive|shell|plan|cost
 "title":"Flash to hardware?",
 "detail":"Will write 38.4 KB via SWD…",
 "command":"STM32_Programmer_CLI -c port=SWD …",
 "tool_use_id":"toolu_…"}
```

**Browser → Server**:

```jsonc
{"type":"confirm.reply", "id":"cf_…", "approve":true,  "patch":null}
{"type":"confirm.reply", "id":"cf_…", "approve":false, "reason":"port locked"}
{"type":"stop"}
```

---

## 5. Subprocess orchestration

```python
# runner.py — sketch, not final code
import asyncio, json
from pathlib import Path

CMD = ["python", "run_claude.py", "--print", "--output-format", "stream-json", "--verbose"]

async def start_run(workspace: Path, args: list[str], prompt: str, on_event):
    proc = await asyncio.create_subprocess_exec(
        "python", "run_claude.py", *args, prompt,
        cwd=str(workspace),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env={**os.environ, "PYTHONUNBUFFERED": "1"},
    )
    async def pump(stream, kind):
        async for line in stream:
            line = line.decode("utf-8", "replace").rstrip()
            if not line: continue
            try:
                evt = json.loads(line)              # stream-json from claude CLI
                async for out in classify(evt):     # may yield confirm.request
                    await on_event(out)
            except json.JSONDecodeError:
                await on_event({"type":"text","text":line,"stream":kind})
    await asyncio.gather(pump(proc.stdout,"stdout"), pump(proc.stderr,"stderr"))
    rc = await proc.wait()
    return rc
```

Key points:

- Use `--output-format stream-json` (already supported by `run_claude.py`) — never parse the human-readable colored output.
- Set `PYTHONUNBUFFERED=1` so events arrive as soon as Python flushes them.
- Pipe both stdout and stderr; the script is chatty on stderr for the [stderr] forwarded lines.

---

## 6. Confirmation gate

The trick: `run_claude.py` doesn't know about the browser. Two complementary strategies — implement **both** for robustness, prefer #2:

### 6.1 Strategy A — Pre-flight classifier (always-on)

Bridge inspects every `tool_use` event before forwarding. If `category == "flash"` (any `STM32_Programmer_CLI -w` / `openocd … program`), `category == "destructive"` (`rm -rf`, `Edit` on a critical file), or `category == "shell"` for unknown long commands, the bridge:

1. Buffers the event without forwarding to the user-facing trace yet.
2. Emits `confirm.request` to the browser.
3. Waits up to N seconds for `confirm.reply`.
4. If approved → forwards the original `tool_use` event and lets the subprocess proceed normally.
5. If rejected → sends SIGTERM to the subprocess, emits `error` event.

> ⚠️ **Caveat:** approving here does **not** prevent the subprocess from running the command — by the time the bridge sees the `tool_use` event, Claude Code CLI has already executed it. So Strategy A really only gates the **display** and **post-hoc continuation**, not the side effect itself.

### 6.2 Strategy B — Human-in-the-loop via permission flag (recommended)

`run_claude.py` currently passes `--dangerously-skip-permissions` to the Claude CLI. The bridge invokes a **second variant** of `run_claude.py` indirectly:

1. Bridge writes a `bridge_runner.py` shim **alongside** (not modifying) `run_claude.py`. The shim imports `run_claude` and re-exports `main()` but strips `--dangerously-skip-permissions` from `argv` before delegation. Since `run_claude.py` is invoked as a subprocess of the shim with that flag removed, the underlying Claude CLI now prompts for permission on each tool call.
2. The bridge intercepts those prompts on stdin/stdout and routes them to the browser.

This requires the Claude CLI to support `--permission-mode prompt` or similar. Verify against your installed version; adjust the shim accordingly. If unavailable, fall back to Strategy A and accept its post-hoc nature.

### 6.3 Categories

| Category       | Heuristic                                                                 |
|----------------|---------------------------------------------------------------------------|
| `flash`        | `STM32_Programmer_CLI -w`, `openocd … program`, `pyocd flash`             |
| `destructive`  | `rm -rf`, `Edit/Write` on `STATE.md`, `CONTEXT.md`, or files outside repo |
| `shell`        | Any `Bash` not on an allow-list (cmake, ninja, ctest, git, ls)            |
| `plan`         | Emitted by bridge after first `text` block on a new run, before any tools |
| `cost`         | When estimated tokens-so-far exceed user-set budget                        |

The list lives in `ccorebridge/confirm.py`. Users override it in `~/.ccorebridge/config.toml`.

---

## 7. Project list

`/api/projects` walks `<workspace>/projects/` and returns:

```json
[
  {
    "id": "09-sms-modem",
    "name": "09-sms-modem",
    "mcu": "STM32H573",
    "status_label": "Build clean · flash pending",
    "summary": "first non-empty paragraph of STATE.md after '## Status'",
    "files": 47,
    "last_run": "2026-04-28T12:01:11Z",
    "iterations": 12
  }
]
```

`mcu` is parsed from `CLAUDE.md` (workspace-level) or per-project `STATE.md` if specified. `iterations` comes from a `.ccorebridge/runs.jsonl` per-project log the bridge maintains.

---

## 8. Auth (local only)

On first launch, bridge generates a 32-byte random token, writes it to `~/.ccorebridge/token` (mode 0600), and prints it to its launching terminal. The browser's login page reads it from `localStorage` after first paste, then sends it on every WebSocket open. No server-side accounts. No remote.

---

## 9. Trace persistence

For each run the bridge writes `<workspace>/.ccorebridge/runs/<run_id>.jsonl` containing every event in order. This lets the browser refresh-and-resume, and lets users diff runs.

---

## 10. Acceptance checklist

- [ ] `ccorebridge ~/DirectClaude` starts the service on `127.0.0.1:8765`.
- [ ] Visiting `http://127.0.0.1:8765/` serves CCoreAi.html.
- [ ] Login form takes the token printed at startup, advances to project list.
- [ ] Project list reflects real `projects/*/STATE.md` content.
- [ ] Pressing **Run** in the workspace spawns the subprocess and streams events live.
- [ ] A `STM32_Programmer_CLI -w` invocation pauses the trace and shows the confirm card.
- [ ] **Approve** lets the run continue; **Reject** kills the subprocess cleanly.
- [ ] Closing the browser tab does NOT kill an in-flight run; reopening reconnects to the same WS.
- [ ] On `run_claude.py` exit, `run.end` carries the exit code and final cost.
- [ ] `run_claude.py`, `skill_loader.py`, and any file under `skills/` is **byte-identical** before and after a run.

---

## 11. Out-of-scope for the POC

- Multi-workspace switching (one workspace per bridge process for now)
- OAuth / SSO / cloud auth
- Multi-user collaborative runs
- Editing project files from the browser (read-only browse only)
- Native serial trace from the board into the UI (mocked in v1)

These are listed in the product vision doc as Phase 2+.
