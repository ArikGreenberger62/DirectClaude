"""Subprocess orchestration + stream-json parsing.

The bridge spawns `python run_claude.py … --output-format stream-json` and
forwards events to a per-run async queue. Browser clients connect to a
WebSocket and receive every event in order. Confirmation gating buffers
risky `tool` events and waits for the user to approve before forwarding.
"""
from __future__ import annotations

import asyncio
import json
import os
import re
import secrets
import signal
import sys
import time

_ANSI_RE = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")


def _strip_ansi(s: str) -> str:
    return _ANSI_RE.sub("", s) if s else s
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Awaitable, Callable

from . import confirm as confirm_mod
from . import projects as projects_mod


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _now_hhmmss() -> str:
    # Local time HH:MM:SS — what the SPA's "time" field expects.
    return datetime.now().strftime("%H:%M:%S")


def _rid() -> str:
    return "r_" + secrets.token_hex(6)


def _cid() -> str:
    return "cf_" + secrets.token_hex(5)


@dataclass
class Run:
    run_id: str
    project: str | None
    prompt: str
    args: dict[str, Any]
    workspace: Path
    started_at: str = field(default_factory=_now_iso)
    ended_at: str | None = None
    exit_code: int | None = None
    cost_usd: float = 0.0
    turns: int = 0
    proc: asyncio.subprocess.Process | None = None
    events: list[dict[str, Any]] = field(default_factory=list)
    log_path: Path | None = None
    # Subscribers: each is an asyncio.Queue receiving every emitted event.
    subscribers: set[asyncio.Queue] = field(default_factory=set)
    # Pending confirmation: id → Future resolved by /api/run/<id>/confirm or WS.
    pending: dict[str, asyncio.Future] = field(default_factory=dict)
    finished: asyncio.Event = field(default_factory=asyncio.Event)
    state: str = "starting"  # starting | running | waiting | done | error | stopped


class RunnerManager:
    """Singleton per-bridge process. Holds the active runs."""

    def __init__(self, workspace: Path):
        self.workspace = workspace
        self.runs: dict[str, Run] = {}
        self._lock = asyncio.Lock()

    # ─────────────────────────────────────────────────────────────────────
    async def start(self, body: dict[str, Any]) -> Run:
        """Spawn run_claude.py and return the Run handle (events stream async)."""
        async with self._lock:
            for r in self.runs.values():
                if r.state in ("starting", "running", "waiting"):
                    raise RuntimeError(
                        f"another run is active ({r.run_id}) — stop it first")

        prompt = body.get("prompt") or ""
        if not prompt.strip():
            raise ValueError("prompt is required")
        project = body.get("project") or None
        run = Run(
            run_id=_rid(),
            project=project,
            prompt=prompt,
            args=body,
            workspace=self.workspace,
        )
        log_dir = self.workspace / ".ccorebridge" / "runs"
        log_dir.mkdir(parents=True, exist_ok=True)
        run.log_path = log_dir / f"{run.run_id}.jsonl"
        self.runs[run.run_id] = run

        argv = self._build_argv(body, prompt, project)
        env = {**os.environ, "PYTHONUNBUFFERED": "1", "PYTHONIOENCODING": "utf-8"}
        # Inject API key if available.
        key_file = self.workspace / "ApiKeyArik"
        if "ANTHROPIC_API_KEY" not in env and key_file.exists():
            try:
                env["ANTHROPIC_API_KEY"] = key_file.read_text(encoding="utf-8").strip()
            except OSError:
                pass

        try:
            run.proc = await asyncio.create_subprocess_exec(
                *argv,
                cwd=str(self.workspace),
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=env,
            )
        except FileNotFoundError as e:
            run.state = "error"
            run.ended_at = _now_iso()
            run.exit_code = 127
            await self._emit(run, {
                "type": "error",
                "message": f"failed to spawn run_claude.py: {e}",
            })
            run.finished.set()
            return run

        run.state = "running"
        await self._emit(run, {
            "type": "run.start",
            "run_id": run.run_id,
            "started_at": run.started_at,
            "project": project,
            "prompt": prompt,
            "argv": argv,
        })
        # Pump stdout / stderr in the background.
        asyncio.create_task(self._pump(run))
        return run

    def _build_argv(self, body: dict[str, Any], prompt: str,
                    project: str | None) -> list[str]:
        py = sys.executable or "python"
        # run_claude.py emits stream-json by default; we only need to forward args.
        argv: list[str] = [py, "run_claude.py"]
        if project:
            argv += ["--continue", project]
        if body.get("task"):
            argv += ["--task", str(body["task"])]
        if body.get("max_iterations"):
            argv += ["--max-iterations", str(int(body["max_iterations"]))]
        if body.get("model"):
            argv += ["--model", str(body["model"])]
        if body.get("effort"):
            argv += ["--effort", str(body["effort"])]
        argv.append(prompt)
        return argv

    # ─────────────────────────────────────────────────────────────────────
    async def _pump(self, run: Run) -> None:
        assert run.proc and run.proc.stdout and run.proc.stderr
        try:
            await asyncio.gather(
                self._pump_stream(run, run.proc.stdout, "stdout"),
                self._pump_stream(run, run.proc.stderr, "stderr"),
            )
        except Exception as e:  # noqa: BLE001
            await self._emit(run, {"type": "error", "message": f"pump crashed: {e}"})
        rc = await run.proc.wait()
        run.exit_code = rc
        run.ended_at = _now_iso()
        run.state = "done" if rc == 0 else ("stopped" if rc in (-15, -2, 130) else "error")
        await self._emit(run, {
            "type": "run.end",
            "exit_code": rc,
            "cost_usd": run.cost_usd,
            "turns": run.turns,
            "ended_at": run.ended_at,
        })
        # Resolve any pending confirmations to unblock waiters.
        for fut in list(run.pending.values()):
            if not fut.done():
                fut.set_result({"approve": False, "reason": "run ended"})
        run.pending.clear()
        run.finished.set()
        # Append a per-project run record so the project list can show iterations.
        if run.project:
            try:
                projects_mod.append_run_log(self.workspace, {
                    "project": run.project,
                    "run_id": run.run_id,
                    "started_at": run.started_at,
                    "ended_at": run.ended_at,
                    "exit_code": rc,
                    "cost_usd": run.cost_usd,
                    "turns": run.turns,
                })
            except OSError:
                pass

    async def _pump_stream(self, run: Run, stream: asyncio.StreamReader,
                            kind: str) -> None:
        async for raw in stream:
            line = raw.decode("utf-8", "replace").rstrip()
            if not line:
                continue
            line = _strip_ansi(line)
            # stderr is unstructured — forward as a "text" event with stream tag.
            if kind == "stderr":
                txt = line
                if txt.startswith("[stderr] "):
                    txt = txt[len("[stderr] "):]
                await self._emit(run, {
                    "type": "text",
                    "stream": "stderr",
                    "text": txt,
                    "ts": _now_iso(),
                    "time": _now_hhmmss(),
                })
                continue
            try:
                evt = json.loads(line)
            except json.JSONDecodeError:
                await self._emit(run, {
                    "type": "text",
                    "stream": "stdout",
                    "text": line,
                    "ts": _now_iso(),
                    "time": _now_hhmmss(),
                })
                continue
            await self._handle_claude_event(run, evt)

    # ─────────────────────────────────────────────────────────────────────
    async def _handle_claude_event(self, run: Run, evt: dict[str, Any]) -> None:
        """Translate a single stream-json event from claude CLI into our schema."""
        et = evt.get("type", "")
        if et == "system" and evt.get("subtype") == "init":
            await self._emit(run, {
                "type": "session",
                "model": evt.get("model", "?"),
                "tools": evt.get("tools"),
                "ts": _now_iso(),
                "time": _now_hhmmss(),
                "text": f"Session started · model: {evt.get('model','?')}",
            })
            return

        if et == "assistant":
            content = (evt.get("message") or {}).get("content") or []
            for block in content:
                btype = block.get("type", "")
                if btype == "text":
                    txt = (block.get("text") or "").strip()
                    if txt:
                        await self._emit(run, {
                            "type": "text",
                            "stream": "assistant",
                            "text": txt,
                            "ts": _now_iso(),
                            "time": _now_hhmmss(),
                        })
                elif btype == "tool_use":
                    await self._handle_tool_use(run, block)
            return

        if et == "user":
            # Tool results live under user messages.
            content = (evt.get("message") or {}).get("content") or []
            for block in content:
                if block.get("type") == "tool_result":
                    txt = ""
                    inner = block.get("content")
                    if isinstance(inner, list):
                        for part in inner:
                            if part.get("type") == "text":
                                txt += part.get("text", "")
                    elif isinstance(inner, str):
                        txt = inner
                    await self._emit(run, {
                        "type": "result",
                        "tool_use_id": block.get("tool_use_id"),
                        "text": txt,
                        "ts": _now_iso(),
                        "time": _now_hhmmss(),
                    })
            return

        if et == "result":
            # Final result envelope.
            cost = evt.get("cost_usd")
            if isinstance(cost, (int, float)):
                run.cost_usd = float(cost)
            turns = evt.get("num_turns")
            if isinstance(turns, int):
                run.turns = turns
            sub = evt.get("subtype", "")
            text = evt.get("result") or ""
            await self._emit(run, {
                "type": "result.final",
                "subtype": sub,
                "text": text,
                "cost_usd": run.cost_usd,
                "turns": run.turns,
                "ts": _now_iso(),
                "time": _now_hhmmss(),
            })
            return

        if et == "error":
            await self._emit(run, {
                "type": "error",
                "message": evt.get("message", json.dumps(evt))[:1000],
                "ts": _now_iso(),
                "time": _now_hhmmss(),
            })
            return

        # Unknown — pass through under "meta" so nothing is dropped silently.
        await self._emit(run, {
            "type": "meta",
            "key": et or "unknown",
            "value": evt,
            "ts": _now_iso(),
            "time": _now_hhmmss(),
        })

    async def _handle_tool_use(self, run: Run, block: dict[str, Any]) -> None:
        tool = block.get("name", "?")
        tool_input = block.get("input") or {}
        tool_use_id = block.get("id") or block.get("tool_use_id") or ""
        decision = confirm_mod.classify(tool, tool_input)
        run.turns += 1
        tool_event = {
            "type": "tool",
            "name": tool,
            "input": tool_input,
            "tool_use_id": tool_use_id,
            "ts": _now_iso(),
            "time": _now_hhmmss(),
            "needs_confirm": decision.needs_confirm,
            "category": decision.category,
        }

        if not decision.needs_confirm:
            await self._emit(run, tool_event)
            return

        # Gate: emit confirm.request, hold the tool event until reply.
        cf_id = _cid()
        prev_state = run.state
        run.state = "waiting"
        loop = asyncio.get_running_loop()
        fut: asyncio.Future = loop.create_future()
        run.pending[cf_id] = fut
        await self._emit(run, {
            "type": "confirm.request",
            "id": cf_id,
            "category": decision.category,
            "title": decision.title,
            "detail": decision.detail,
            "command": decision.command,
            "tool_use_id": tool_use_id,
            "ts": _now_iso(),
            "time": _now_hhmmss(),
        })
        try:
            reply = await fut
        except asyncio.CancelledError:
            reply = {"approve": False, "reason": "cancelled"}
        run.pending.pop(cf_id, None)
        run.state = prev_state if prev_state == "running" else "running"

        if reply.get("approve"):
            await self._emit(run, {
                "type": "confirm.resolved",
                "id": cf_id,
                "approve": True,
                "ts": _now_iso(),
                "time": _now_hhmmss(),
            })
            await self._emit(run, tool_event)
        else:
            reason = reply.get("reason") or "rejected"
            await self._emit(run, {
                "type": "confirm.resolved",
                "id": cf_id,
                "approve": False,
                "reason": reason,
                "ts": _now_iso(),
                "time": _now_hhmmss(),
            })
            await self._emit(run, {
                "type": "error",
                "message": f"User rejected confirmation: {reason}. Stopping run.",
                "ts": _now_iso(),
                "time": _now_hhmmss(),
            })
            await self.stop(run.run_id)

    # ─────────────────────────────────────────────────────────────────────
    async def confirm(self, run_id: str, cf_id: str, approve: bool,
                       reason: str | None = None) -> bool:
        run = self.runs.get(run_id)
        if not run:
            return False
        fut = run.pending.get(cf_id)
        if not fut or fut.done():
            return False
        fut.set_result({"approve": approve, "reason": reason})
        return True

    async def stop(self, run_id: str) -> bool:
        run = self.runs.get(run_id)
        if not run or not run.proc:
            return False
        if run.proc.returncode is not None:
            return True
        try:
            if sys.platform == "win32":
                run.proc.terminate()
            else:
                run.proc.send_signal(signal.SIGTERM)
        except ProcessLookupError:
            return True
        # Resolve pending confirmations so awaiters wake up.
        for fut in list(run.pending.values()):
            if not fut.done():
                fut.set_result({"approve": False, "reason": "stopped"})
        return True

    def get(self, run_id: str) -> Run | None:
        return self.runs.get(run_id)

    # ─────────────────────────────────────────────────────────────────────
    async def _emit(self, run: Run, evt: dict[str, Any]) -> None:
        """Append to the run buffer and broadcast to every subscriber."""
        evt = dict(evt)
        evt.setdefault("ts", _now_iso())
        evt.setdefault("time", _now_hhmmss())
        run.events.append(evt)
        # Persist to per-run jsonl log.
        if run.log_path is not None:
            try:
                with run.log_path.open("a", encoding="utf-8") as f:
                    f.write(json.dumps(evt, ensure_ascii=False, default=str) + "\n")
            except OSError:
                pass
        # Track lifecycle states from our own emitted lifecycle events.
        if evt.get("type") == "run.end":
            pass
        for q in list(run.subscribers):
            try:
                q.put_nowait(evt)
            except asyncio.QueueFull:
                pass

    def subscribe(self, run: Run) -> asyncio.Queue:
        q: asyncio.Queue = asyncio.Queue(maxsize=1024)
        run.subscribers.add(q)
        return q

    def unsubscribe(self, run: Run, q: asyncio.Queue) -> None:
        run.subscribers.discard(q)
