"""Uvicorn launcher.

Usage::

    ccorebridge ~/DirectClaude
    ccorebridge ~/DirectClaude --host 127.0.0.1 --port 8765

Or directly::

    python -m ccorebridge ~/DirectClaude
"""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import uvicorn

from .server import create_app


def cli() -> int:
    parser = argparse.ArgumentParser(
        prog="ccorebridge",
        description="Local bridge between the CCoreAi browser SPA and run_claude.py.",
    )
    parser.add_argument(
        "workspace",
        nargs="?",
        default=os.environ.get("CCB_WORKSPACE")
                or str(Path(__file__).resolve().parents[3]),
        help="Path to the DirectClaude workspace (the folder containing run_claude.py)",
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--reload", action="store_true",
                        help="enable uvicorn auto-reload (dev)")
    args = parser.parse_args()

    workspace = Path(args.workspace).expanduser().resolve()
    if not workspace.is_dir():
        print(f"workspace not a directory: {workspace}", file=sys.stderr)
        return 2

    app = create_app(workspace)
    uvicorn.run(app, host=args.host, port=args.port, reload=args.reload,
                log_level="info")
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(cli())
