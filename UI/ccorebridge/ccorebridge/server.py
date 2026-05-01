"""FastAPI routes — REST + WebSocket.

The contract here matches ARCHITECTURE.md §4. We keep the surface narrow:

  GET  /api/health
  POST /api/auth                  → exchanges the local token for a session
  GET  /api/projects
  GET  /api/projects/{id}
  POST /api/run                   → starts a run, returns {run_id}
  POST /api/run/{id}/confirm
  POST /api/run/{id}/stop
  GET  /api/run/{id}/log          → JSON-Lines transcript

  GET  /ws/run/{run_id}?token=…   → live event stream

Static SPA is mounted at "/" so the user can visit http://127.0.0.1:8765/.
"""
from __future__ import annotations

import asyncio
import json
from pathlib import Path
from typing import Any

from fastapi import (Depends, FastAPI, HTTPException, Request, WebSocket,
                     WebSocketDisconnect, status)
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import (FileResponse, JSONResponse, PlainTextResponse,
                                 RedirectResponse, Response)
from fastapi.staticfiles import StaticFiles

from . import __version__
from . import auth as auth_mod
from . import projects as projects_mod
from .runner import RunnerManager


def create_app(workspace: Path) -> FastAPI:
    workspace = workspace.resolve()
    if not (workspace / "run_claude.py").exists():
        raise SystemExit(
            f"workspace missing run_claude.py: {workspace}\n"
            f"Pass the path to your DirectClaude folder, e.g.\n"
            f"  ccorebridge ~/DirectClaude")

    token = auth_mod.load_or_create_token()
    runner = RunnerManager(workspace)

    app = FastAPI(title="CCoreBridge", version=__version__)
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["http://127.0.0.1:8765", "http://localhost:8765"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # ── Auth helper ──────────────────────────────────────────────────────
    def _check_auth(request: Request) -> None:
        # Accept the token via Authorization: Bearer or X-CCB-Token header.
        hdr = request.headers.get("authorization", "")
        if hdr.lower().startswith("bearer "):
            supplied = hdr.split(" ", 1)[1].strip()
        else:
            supplied = request.headers.get("x-ccb-token", "").strip()
        if not auth_mod.constant_time_eq(supplied, token):
            raise HTTPException(status_code=401, detail="invalid or missing token")

    # ── Health ───────────────────────────────────────────────────────────
    @app.get("/api/health")
    async def health() -> dict[str, Any]:
        return {
            "ok": True,
            "version": __version__,
            "workspace": str(workspace),
        }

    # ── Auth ─────────────────────────────────────────────────────────────
    @app.post("/api/auth")
    async def authenticate(body: dict[str, Any]) -> dict[str, Any]:
        supplied = (body or {}).get("token", "").strip()
        if not auth_mod.constant_time_eq(supplied, token):
            raise HTTPException(status_code=401, detail="invalid token")
        # We don't issue a separate session: the browser keeps the token and
        # sends it on every request. This response is just an "ok".
        return {"session": "ok", "workspace": str(workspace)}

    # ── Projects ─────────────────────────────────────────────────────────
    @app.get("/api/projects")
    async def projects(request: Request) -> list[dict[str, Any]]:
        _check_auth(request)
        return projects_mod.list_projects(workspace)

    @app.get("/api/projects/{project_id}")
    async def project_one(project_id: str, request: Request) -> dict[str, Any]:
        _check_auth(request)
        info = projects_mod.get_project(workspace, project_id)
        if info is None:
            raise HTTPException(status_code=404, detail="project not found")
        return info

    # ── Runs ─────────────────────────────────────────────────────────────
    @app.post("/api/run")
    async def run_start(body: dict[str, Any], request: Request) -> dict[str, Any]:
        _check_auth(request)
        try:
            run = await runner.start(body)
        except (ValueError, RuntimeError) as e:
            raise HTTPException(status_code=400, detail=str(e))
        return {"run_id": run.run_id, "started_at": run.started_at}

    @app.post("/api/run/{run_id}/confirm")
    async def run_confirm(run_id: str, body: dict[str, Any],
                          request: Request) -> dict[str, Any]:
        _check_auth(request)
        cf_id = body.get("id") or body.get("cf_id")
        action = (body.get("action") or "").lower()
        approve = action == "approve" or bool(body.get("approve"))
        reason = body.get("reason")
        if not cf_id:
            raise HTTPException(status_code=400, detail="confirm id required")
        ok = await runner.confirm(run_id, cf_id, approve=approve, reason=reason)
        if not ok:
            raise HTTPException(status_code=404, detail="no pending confirm")
        return {"ok": True}

    @app.post("/api/run/{run_id}/stop")
    async def run_stop(run_id: str, request: Request) -> dict[str, Any]:
        _check_auth(request)
        ok = await runner.stop(run_id)
        if not ok:
            raise HTTPException(status_code=404, detail="run not found")
        return {"ok": True}

    @app.get("/api/run/{run_id}/log")
    async def run_log(run_id: str, request: Request) -> JSONResponse:
        _check_auth(request)
        run = runner.get(run_id)
        if not run:
            raise HTTPException(status_code=404, detail="run not found")
        return JSONResponse(run.events)

    # ── WebSocket ────────────────────────────────────────────────────────
    @app.websocket("/ws/run/{run_id}")
    async def run_ws(ws: WebSocket, run_id: str) -> None:
        # Token via query string ?token=…
        supplied = ws.query_params.get("token", "")
        if not auth_mod.constant_time_eq(supplied, token):
            await ws.close(code=4401)
            return
        run = runner.get(run_id)
        if not run:
            await ws.close(code=4404)
            return
        await ws.accept()
        # Replay backlog so a fresh client (or reload) sees full transcript.
        for evt in list(run.events):
            try:
                await ws.send_text(json.dumps(evt, default=str))
            except Exception:
                await ws.close()
                return

        q = runner.subscribe(run)

        async def reader() -> None:
            try:
                while True:
                    msg = await ws.receive_text()
                    try:
                        data = json.loads(msg)
                    except json.JSONDecodeError:
                        continue
                    mtype = data.get("type")
                    if mtype == "confirm.reply":
                        await runner.confirm(
                            run.run_id, data.get("id", ""),
                            approve=bool(data.get("approve")),
                            reason=data.get("reason"))
                    elif mtype == "stop":
                        await runner.stop(run.run_id)
                    # Future: client→server text messages.
            except WebSocketDisconnect:
                pass
            except Exception:
                pass

        async def writer() -> None:
            try:
                while True:
                    if run.finished.is_set() and q.empty():
                        # Drain anything left, then exit.
                        return
                    try:
                        evt = await asyncio.wait_for(q.get(), timeout=1.0)
                    except asyncio.TimeoutError:
                        if run.finished.is_set() and q.empty():
                            return
                        continue
                    await ws.send_text(json.dumps(evt, default=str))
            except WebSocketDisconnect:
                pass
            except Exception:
                pass

        try:
            await asyncio.gather(reader(), writer())
        finally:
            runner.unsubscribe(run, q)
            try:
                await ws.close()
            except Exception:
                pass

    # ── Static SPA ──────────────────────────────────────────────────────
    static_dir = Path(__file__).parent / "static"
    if not static_dir.exists():
        static_dir.mkdir(parents=True, exist_ok=True)
    app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")

    @app.get("/")
    async def root() -> RedirectResponse:
        return RedirectResponse(url="/app/")

    @app.get("/app")
    async def app_root_no_slash() -> RedirectResponse:
        return RedirectResponse(url="/app/")

    @app.get("/app/")
    async def app_index() -> Any:
        idx = static_dir / "CCoreAi.html"
        if not idx.exists():
            return PlainTextResponse(
                "CCoreAi.html missing in static/ — run staging step.",
                status_code=500)
        return PlainTextResponse(idx.read_text(encoding="utf-8"),
                                  media_type="text/html; charset=utf-8")

    # Serve sibling files referenced by the SPA (styles.css, components/, etc.).
    _MEDIA = {
        ".css": "text/css; charset=utf-8",
        ".js": "application/javascript; charset=utf-8",
        ".jsx": "application/javascript; charset=utf-8",
        ".mjs": "application/javascript; charset=utf-8",
        ".html": "text/html; charset=utf-8",
        ".json": "application/json; charset=utf-8",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".png": "image/png",
        ".svg": "image/svg+xml",
    }

    @app.get("/app/{path:path}")
    async def app_assets(path: str) -> Any:
        target = (static_dir / path).resolve()
        try:
            target.relative_to(static_dir.resolve())
        except ValueError:
            raise HTTPException(status_code=403, detail="forbidden")
        if not target.exists() or target.is_dir():
            raise HTTPException(status_code=404, detail="not found")
        media = _MEDIA.get(target.suffix.lower(), "application/octet-stream")
        return FileResponse(str(target), media_type=media)

    # Convenience: print token to log on startup.
    @app.on_event("startup")
    async def _startup() -> None:
        print(f"\n  CCoreBridge {__version__}")
        print(f"  Workspace: {workspace}")
        print(f"  Token:     {token}")
        print(f"  URL:       http://127.0.0.1:8765/\n")

    return app
