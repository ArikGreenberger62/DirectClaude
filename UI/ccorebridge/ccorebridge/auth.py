"""Local-only token auth.

On first launch we generate a 32-byte random token and write it to
~/.ccorebridge/token (mode 0600 on POSIX). The browser reads/pastes it on the
login page and then sends it on every WebSocket connect.

There are no remote calls, no accounts. The token only protects against
other local users on the same machine accidentally hitting the bridge.
"""
from __future__ import annotations

import os
import secrets
import sys
from pathlib import Path


def token_path() -> Path:
    return Path.home() / ".ccorebridge" / "token"


def load_or_create_token() -> str:
    p = token_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    if p.exists():
        tok = p.read_text(encoding="utf-8").strip()
        if tok:
            return tok
    tok = secrets.token_urlsafe(32)
    p.write_text(tok, encoding="utf-8")
    if sys.platform != "win32":
        try:
            os.chmod(p, 0o600)
        except OSError:
            pass
    return tok


def constant_time_eq(a: str, b: str) -> bool:
    return secrets.compare_digest(a or "", b or "")
