"""Workspace project listing — reads <workspace>/projects/*."""
from __future__ import annotations

import json
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


# Match the first non-empty paragraph after a "## Status" header.
_STATUS_RE = re.compile(r"^##\s+Status\s*\n+([^\n#][^\n]*(?:\n(?!\s*\n)[^\n]*)*)",
                        re.MULTILINE)
_MCU_RE = re.compile(r"\*\*MCU:\*\*\s*([^\s|]+)|MCU:\s*([A-Z0-9]+)", re.IGNORECASE)


def _summarise_state(text: str) -> str:
    m = _STATUS_RE.search(text)
    if not m:
        return ""
    return m.group(1).strip().replace("\n", " ")


def _detect_mcu(workspace: Path, state_text: str | None) -> str:
    if state_text:
        m = _MCU_RE.search(state_text)
        if m:
            return (m.group(1) or m.group(2) or "").strip()
    claude_md = workspace / "CLAUDE.md"
    if claude_md.exists():
        m = _MCU_RE.search(claude_md.read_text(encoding="utf-8", errors="replace"))
        if m:
            return (m.group(1) or m.group(2) or "").strip()
    return "auto-detect"


def _last_run(workspace: Path, project_id: str) -> tuple[str | None, int]:
    log = workspace / ".ccorebridge" / "runs.jsonl"
    if not log.exists():
        return None, 0
    last_ts: str | None = None
    iters = 0
    try:
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line.strip():
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if obj.get("project") != project_id:
                continue
            iters += 1
            ts = obj.get("ended_at") or obj.get("started_at")
            if ts:
                last_ts = ts
    except OSError:
        return None, 0
    return last_ts, iters


def _count_files(folder: Path) -> int:
    if not folder.exists():
        return 0
    count = 0
    for _ in folder.rglob("*"):
        count += 1
        if count > 9999:
            break
    return count


def _status_label(state_text: str) -> str:
    head = (state_text or "").lower()
    if "verified" in head and "hardware" in head:
        return "Verified on hardware"
    if "build clean" in head and ("flash" in head or "verify pending" in head):
        return "Build clean · flash pending"
    if "build clean" in head:
        return "Build clean"
    if "fail" in head:
        return "Failure recorded"
    return "Idle"


def _short_id_to_status(label: str) -> str:
    head = label.lower()
    if "verified" in head:
        return "verified"
    if "fail" in head or "error" in head:
        return "error"
    if "build clean" in head:
        return "build-clean"
    return "idle"


def list_projects(workspace: Path) -> list[dict[str, Any]]:
    projects_dir = workspace / "projects"
    if not projects_dir.exists():
        return []
    out: list[dict[str, Any]] = []
    for d in sorted(projects_dir.iterdir()):
        if not d.is_dir():
            continue
        if d.name.startswith(".") or d.name == "build":
            continue
        state = d / "STATE.md"
        text = state.read_text(encoding="utf-8", errors="replace") if state.exists() else ""
        summary = _summarise_state(text)
        last_run, iterations = _last_run(workspace, d.name)
        status_label = _status_label(text)
        out.append({
            "id": d.name,
            "name": d.name,
            "mcu": _detect_mcu(workspace, text),
            "status": _short_id_to_status(status_label),
            "statusLabel": status_label,
            "summary": summary or "(no STATE.md summary)",
            "lastRun": last_run,
            "iterations": iterations,
            "files": _count_files(d),
        })
    return out


def get_project(workspace: Path, project_id: str) -> dict[str, Any] | None:
    proj = workspace / "projects" / project_id
    if not proj.is_dir():
        return None
    state = proj / "STATE.md"
    text = state.read_text(encoding="utf-8", errors="replace") if state.exists() else ""
    last_run, iterations = _last_run(workspace, project_id)
    status_label = _status_label(text)
    return {
        "id": project_id,
        "name": project_id,
        "mcu": _detect_mcu(workspace, text),
        "status": _short_id_to_status(status_label),
        "statusLabel": status_label,
        "summary": _summarise_state(text) or "",
        "stateMd": text,
        "lastRun": last_run,
        "iterations": iterations,
        "files": _count_files(proj),
    }


def append_run_log(workspace: Path, record: dict[str, Any]) -> None:
    log_dir = workspace / ".ccorebridge"
    log_dir.mkdir(parents=True, exist_ok=True)
    record = {**record}
    record.setdefault("ended_at", datetime.now(timezone.utc).isoformat(timespec="seconds"))
    with (log_dir / "runs.jsonl").open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")
