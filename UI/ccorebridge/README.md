# CCoreBridge

Local bridge between the **CCoreAi** browser SPA and the existing
`run_claude.py` script in your DirectClaude workspace.

The bridge is a thin FastAPI process that:

1. Serves the SPA on `http://127.0.0.1:8765/`.
2. Spawns `run_claude.py` as a subprocess for each browser-initiated run,
   pipes its `--output-format stream-json` output, and forwards every event to
   the browser over a WebSocket.
3. Inspects every `tool_use` event and asks the user to **Approve** or
   **Reject** risky calls (flash, destructive shell, edits to STATE/CONTEXT).
4. Persists every event to `<workspace>/.ccorebridge/runs/<run_id>.jsonl` so a
   browser refresh re-attaches to the same run.

`run_claude.py`, `skill_loader.py`, and everything under `skills/` are
**read-only** to the bridge — they are never modified.

---

## Quick start

```powershell
# 1. Install (editable mode is fine).
cd C:\CCoreAI\DirectClaude\DirectClaude\UI\ccorebridge
python -m pip install -e .

# 2. Run, pointing at your DirectClaude workspace.
python -m ccorebridge C:\CCoreAI\DirectClaude\DirectClaude
# or, after install:
ccorebridge C:\CCoreAI\DirectClaude\DirectClaude
```

You will see something like:

```
  CCoreBridge 0.3.1
  Workspace: C:\CCoreAI\DirectClaude\DirectClaude
  Token:     PufEMTW0AyjAC9Yzi7KolJA2K_EOQ37PGZ58HJyeP2c
  URL:       http://127.0.0.1:8765/
```

3. Open `http://127.0.0.1:8765/` in your browser.
4. Paste the token into the **Bridge token** field on the login page.
   The token is also saved at `~/.ccorebridge/token` for next time.

The token is local-only auth — it just keeps another user on the same
machine from accidentally hitting the bridge. There is no remote auth.

---

## What you can do

- **Project list** — built from `<workspace>/projects/*` and each project's
  `STATE.md`. Click a project to open its workspace.
- **Run** — type a prompt and press **Run** (or Cmd/Ctrl + ↵). The bridge
  spawns `python run_claude.py --continue <project> "<prompt>"`. Live trace
  appears in the main pane as events stream in.
- **Confirm** — when the agent runs a risky command (e.g.
  `STM32_Programmer_CLI -w …` or `Edit STATE.md`), the trace pauses on a
  card. **Approve** to let the bridge forward the event; **Reject** to
  SIGTERM the subprocess.
- **Stop** — kills the active run cleanly.
- **Refresh-resume** — closing the browser tab does not kill the run; on
  reopen, the bridge replays the full event log over the WebSocket.

The Tweaks panel (bottom-right) still works for offline previews — pick
**Run state → Run / Confirm / Done** to replay the original `SAMPLE_RUN`
script without touching the bridge.

---

## Architecture

```
┌──────────────────────────┐         ┌──────────────────────────────┐
│  Browser  (CCoreAi.html) │         │  CCoreBridge  (this package) │
│                          │ ◄──WS──►│                              │
│  - bridge.js → window.CCB│   8765  │  FastAPI + uvicorn           │
│  - Login → /api/auth     │         │  ├─ /app    (serves SPA)     │
│  - Projects → /api/...   │         │  ├─ /api/projects            │
│  - Run → /api/run        │         │  ├─ /api/run     (POST)      │
│  - Live events ←  WS     │         │  └─ /ws/run/<run_id>         │
└──────────────────────────┘         │                              │
                                     │  Per-run subprocess:         │
                                     │  python run_claude.py …      │
                                     │  (UNMODIFIED)                │
                                     └──────────────────────────────┘
```

Files:

```
ccorebridge/
├── __main__.py     # uvicorn launcher
├── server.py       # FastAPI routes + WebSocket
├── runner.py       # Subprocess + stream-json parser + confirm gate
├── confirm.py      # Risky tool classifier (Strategy A)
├── projects.py     # STATE.md walker
├── auth.py         # Local token (~/.ccorebridge/token)
└── static/         # CCoreAi.html, bridge.js, components/, styles.css
```

---

## REST contract

| Method | Path                        | Purpose                                      |
|--------|-----------------------------|----------------------------------------------|
| GET    | `/api/health`               | `{ok, version, workspace}` — unauth          |
| POST   | `/api/auth`                 | `{token}` → `{session}`                      |
| GET    | `/api/projects`             | List `projects/*` + STATE.md summary         |
| GET    | `/api/projects/{id}`        | One project + full STATE.md                  |
| POST   | `/api/run`                  | Start a run, returns `{run_id}`              |
| POST   | `/api/run/{id}/confirm`     | `{id, action: "approve"\|"reject"}`          |
| POST   | `/api/run/{id}/stop`        | SIGTERM the subprocess                       |
| GET    | `/api/run/{id}/log`         | Full transcript (JSON array)                 |

All `/api/*` routes except `/api/health` and `/api/auth` require either
`Authorization: Bearer <token>` or `X-CCB-Token: <token>`.

`POST /api/run` body:

```json
{
  "project": "09-sms-modem",
  "prompt": "flash and verify the EG915N power-on trace on COM7",
  "task": "code,build,test",
  "max_iterations": 10,
  "model": "sonnet",
  "effort": "max"
}
```

The bridge translates this into:

```
python run_claude.py --continue 09-sms-modem --task code,build,test \
                     --max-iterations 10 --model sonnet --effort max \
                     "flash and verify…"
```

---

## WebSocket contract

`GET ws://127.0.0.1:8765/ws/run/<run_id>?token=<token>`

Server → Browser events (line-delimited JSON):

```jsonc
{"type":"run.start",       "run_id":"r_…", "started_at":"…", "project":"…", "prompt":"…"}
{"type":"session",         "model":"…", "ts":"…", "time":"HH:MM:SS"}
{"type":"text",            "text":"…", "stream":"assistant|stdout|stderr"}
{"type":"tool",            "name":"Bash", "input":{…}, "tool_use_id":"toolu_…",
                           "needs_confirm":false, "category":null}
{"type":"result",          "text":"…", "tool_use_id":"toolu_…"}
{"type":"result.final",    "subtype":"success", "text":"…", "cost_usd":0.184, "turns":11}
{"type":"confirm.request", "id":"cf_…", "category":"flash",
                           "title":"Flash to hardware?", "detail":"…",
                           "command":"STM32_Programmer_CLI -w …",
                           "tool_use_id":"toolu_…"}
{"type":"confirm.resolved","id":"cf_…", "approve":true}
{"type":"error",           "message":"…"}
{"type":"run.end",         "exit_code":0, "cost_usd":0.184, "turns":11}
```

Browser → Server:

```jsonc
{"type":"confirm.reply", "id":"cf_…", "approve":true}
{"type":"confirm.reply", "id":"cf_…", "approve":false, "reason":"port locked"}
{"type":"stop"}
```

---

## Confirmation gate (Strategy A)

Defined in `ccorebridge/confirm.py`. Categories:

| Category       | Heuristic                                                                 |
|----------------|---------------------------------------------------------------------------|
| `flash`        | `STM32_Programmer_CLI -w`, `openocd … program`, `pyocd flash`             |
| `destructive`  | `rm -rf`, `git reset --hard`, `git push --force`, edits to `STATE.md` /   |
|                | `CONTEXT.md` / `CLAUDE.md` / `run_claude.py` / `skill_loader.py`          |
| `shell`        | Any `Bash` not on the safe-prefix allowlist (cmake, ninja, ctest, git,    |
|                | python tools/trace.py, ST-LINK detection-only `-l st`, …)                 |

> ⚠️ Strategy A is **post-hoc**: by the time the bridge sees a `tool_use`
> event, `run_claude.py`'s underlying claude CLI (invoked with
> `--dangerously-skip-permissions`) has already executed the command. The
> gate controls the displayed trace and whether the run continues, not the
> side effect itself. Strategy B (a permission-prompt shim that wraps
> `run_claude.py`) is the recommended next step — see ARCHITECTURE.md §6.2.

---

## File locations

- **Workspace**          — `<workspace>/` (the folder containing
                           `run_claude.py`)
- **Token**              — `~/.ccorebridge/token` (mode 0600 on POSIX)
- **Run log per project**— `<workspace>/.ccorebridge/runs.jsonl`
- **Run transcript**     — `<workspace>/.ccorebridge/runs/<run_id>.jsonl`

---

## Acceptance checklist (ARCHITECTURE.md §10)

- [x] `ccorebridge ~/DirectClaude` starts the service on `127.0.0.1:8765`.
- [x] `http://127.0.0.1:8765/` redirects to `/app/` and serves CCoreAi.html.
- [x] Login form takes the token printed at startup, advances to project list.
- [x] Project list reflects real `projects/*/STATE.md` content.
- [x] Pressing **Run** spawns the subprocess and streams events live.
- [x] A `STM32_Programmer_CLI -w …` invocation pauses the trace and shows the
  confirm card.
- [x] **Approve** lets the run continue; **Reject** SIGTERMs the subprocess.
- [x] Closing the browser tab does NOT kill an in-flight run; reopening
  reconnects to the same WS and replays the full backlog.
- [x] On `run_claude.py` exit, `run.end` carries the exit code and final cost.
- [x] `run_claude.py`, `skill_loader.py`, and any file under `skills/` is
  byte-identical before and after a run.

---

## Smoke test (manual)

```powershell
# Health
curl http://127.0.0.1:8765/api/health

# Auth
curl -H "Content-Type: application/json" `
  -d "{\"token\":\"$env:TOKEN\"}" `
  http://127.0.0.1:8765/api/auth

# Projects
curl -H "Authorization: Bearer $env:TOKEN" `
  http://127.0.0.1:8765/api/projects
```

Or open the SPA, log in, click a project, type "list files in Core/Inc" and
press **Run** — events should flow into the trace pane within ~1 s.

---

## Known limits / next steps

- Strategy A only. Implement Strategy B by wrapping `run_claude.py` so the
  underlying claude CLI prompts on each tool call.
- One workspace per bridge process. Multi-workspace switching is Phase 2.
- The Tweaks panel mock states still drive the SAMPLE_RUN replay in the
  workspace pane — useful for designing UI without spending API tokens.
