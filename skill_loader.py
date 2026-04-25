#!/usr/bin/env python3
"""
skill_loader.py — Discover, select, and bundle DirectClaude skills.

The loader is the **single source of truth** for "which skills does this task
need?" — it is shared by run_claude.py (when invoking Claude via the CLI) and
can be imported by any other Python entry point. The Claude application itself
reads the same SKILL.md files via the Skill Map in CLAUDE.md, so behaviour is
identical in both modes.

Token-saving design:
  * Skills are **opt-in**: nothing is loaded unless triggered by a keyword or
    by an explicit task_type.
  * Each SKILL.md declares `keywords:` and `task_types:` in its frontmatter.
  * Selection is deterministic — same prompt → same bundle → same prompt-cache
    prefix (Anthropic's 5-min cache stays warm across iterations).
  * Output bundle is built in stable priority order so the cache hashes match
    even when the user appends an extra word to the prompt.

Public API:
    discover_skills(skills_dir)                    -> list[Skill]
    detect_task_types(prompt)                      -> set[str]
    select_skills(skills, prompt, task_types=None) -> list[Skill]
    assemble_bundle(skills, project_state=None)    -> str
    load_project_state(folder, project)            -> str | None
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

# ── Frontmatter parser ────────────────────────────────────────────────────────
_FRONTMATTER_RE = re.compile(r"^---\s*\n(.*?)\n---\s*\n", re.DOTALL)


def _parse_frontmatter(text: str) -> tuple[dict, str]:
    """Tiny YAML-subset parser — handles scalars and `[a, b, c]` lists only."""
    m = _FRONTMATTER_RE.match(text)
    if not m:
        return {}, text
    block, body = m.group(1), text[m.end():]
    meta: dict = {}
    for raw in block.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        key, _, val = line.partition(":")
        key, val = key.strip(), val.strip()
        if val.startswith("[") and val.endswith("]"):
            items = [x.strip().strip("'\"") for x in val[1:-1].split(",")]
            meta[key] = [x for x in items if x]
        else:
            meta[key] = val.strip("'\"")
    return meta, body


# ── Skill record ──────────────────────────────────────────────────────────────
PRIORITY_ORDER = {"base": 0, "tier2": 1, "tier3": 2, "tier4": 3}


@dataclass(frozen=True)
class Skill:
    path: Path
    name: str
    description: str
    keywords: tuple[str, ...]
    task_types: tuple[str, ...]
    priority: str
    body: str

    @property
    def priority_rank(self) -> int:
        return PRIORITY_ORDER.get(self.priority, 99)

    def matches(self, prompt_lc: str, task_types: set[str]) -> bool:
        """Match rules:
          * priority=base → matches whenever any task_type was detected
            (handled by select_skills include_base path).
          * non-base → keyword must appear in the prompt. task_types alone
            is not enough, otherwise unrelated skills like modem/eg915n
            would load on every test/code task.
          * If a non-base skill's task_types do not intersect the detected
            set, it is excluded even if a stray keyword matches (avoids
            loading the modem skill for the bare word "at" etc).
        """
        kw_hit = any(_kw_in(prompt_lc, k) for k in self.keywords)
        if not kw_hit:
            return False
        if task_types and self.task_types:
            return bool(set(self.task_types) & task_types)
        return True


def _kw_in(prompt_lc: str, kw: str) -> bool:
    """Word-boundary keyword match (case-insensitive)."""
    if not kw:
        return False
    kw = kw.lower()
    if not re.match(r"^[\w+./-]+$", kw):
        return kw in prompt_lc
    return re.search(rf"(?<![\w]){re.escape(kw)}(?![\w])", prompt_lc) is not None


# ── Discovery ────────────────────────────────────────────────────────────────
def discover_skills(skills_dir: Path) -> list[Skill]:
    """Find every SKILL.md under skills_dir and parse its frontmatter."""
    skills: list[Skill] = []
    if not skills_dir.exists():
        return skills
    for path in sorted(skills_dir.rglob("SKILL.md")):
        text = path.read_text(encoding="utf-8")
        meta, body = _parse_frontmatter(text)
        skills.append(Skill(
            path=path,
            name=meta.get("name", str(path.parent.name)),
            description=meta.get("description", ""),
            keywords=tuple(k.lower() for k in meta.get("keywords", [])),
            task_types=tuple(meta.get("task_types", [])),
            priority=meta.get("priority", "tier3"),
            body=body.strip(),
        ))
    return skills


# ── Task-type detection (best-effort heuristic) ──────────────────────────────
_TASK_TOKENS: dict[str, tuple[str, ...]] = {
    "arch":  ("scaffold", "skeleton", "architecture", "structure", "design",
              "layout", "new"),
    "code":  ("write", "implement", "driver", "module", "function", "code",
              "feature", "add", "logic", "bug", "refactor"),
    "build": ("build", "compile", "compiles", "compiling", "linker", "link",
              "cmake", "cmakelists", "ninja", "warning", "preset"),
    "test":  ("flash", "trace", "verify", "test", "ctest", "self_test",
              "stlink", "st-link", "com7", "serial"),
}


def detect_task_types(prompt: str) -> set[str]:
    """Scan prompt for task tokens. Multiple tasks may match simultaneously.

    Project-creation phrases ("create … project", "new project") imply the
    full pipeline (arch + code + build + test) so the autonomous workflow
    in CLAUDE.md gets the skills it needs.
    """
    p = prompt.lower()
    tokens = set(re.findall(r"[a-z0-9_+-]+", p))
    detected: set[str] = set()
    for tt, cues in _TASK_TOKENS.items():
        if any(c in tokens for c in cues):
            detected.add(tt)

    # Whole-pipeline trigger: "create … project" / "new project"
    if ("project" in tokens) and ({"create", "new", "scaffold"} & tokens):
        detected.update({"arch", "code", "build", "test"})
    return detected


# ── Selection ────────────────────────────────────────────────────────────────
def select_skills(
    skills: Iterable[Skill],
    prompt: str,
    task_types: set[str] | None = None,
    include_base: bool = True,
) -> list[Skill]:
    """Pick the minimum useful skill set for `prompt`.

    Rules:
      * If task_types is None, auto-detect from the prompt.
      * Always include skills tagged priority=base when include_base=True
        AND at least one task_type was detected (otherwise we'd dump base
        skills on a pure question).
      * A skill matches if any of its keywords appears in the prompt OR
        its task_types intersect the detected/passed set.
      * Output is sorted by (priority_rank, name) — stable for prompt cache.
    """
    if task_types is None:
        task_types = detect_task_types(prompt)
    prompt_lc = prompt.lower()
    selected: list[Skill] = []
    for s in skills:
        if include_base and s.priority == "base" and task_types:
            selected.append(s)
            continue
        if s.matches(prompt_lc, task_types):
            selected.append(s)
    # de-dup + stable order
    seen: set[str] = set()
    unique: list[Skill] = []
    for s in sorted(selected, key=lambda x: (x.priority_rank, x.name)):
        if s.name in seen:
            continue
        seen.add(s.name)
        unique.append(s)
    return unique


# ── Bundle assembler ─────────────────────────────────────────────────────────
def assemble_bundle(
    selected: list[Skill],
    project_state: str | None = None,
    extra_notes: str | None = None,
) -> str:
    """Build a deterministic prompt prefix.

    The bundle is structured for prompt caching: stable content first, the
    user request is appended *after* this block by the caller.
    """
    parts: list[str] = []
    parts.append(
        "## PRE-LOADED SKILLS\n"
        "The skills below have already been resolved for this task.\n"
        "**Do NOT re-read these SKILL.md files** — apply them directly.\n"
    )
    if not selected:
        parts.append("_(No skills triggered for this prompt — proceed with general knowledge of CLAUDE.md.)_\n")
    else:
        for s in selected:
            rel = s.path.as_posix()
            parts.append(f"### {s.name}  \n_(source: `{rel}`)_\n\n{s.body}\n")
    if project_state:
        parts.append("## PROJECT STATE (compact, last-session cache)\n")
        parts.append(project_state.strip() + "\n")
    if extra_notes:
        parts.append("## NOTES\n" + extra_notes.strip() + "\n")
    return "\n---\n\n".join(parts) + "\n"


# ── Project state loader ─────────────────────────────────────────────────────
def load_project_state(workspace: Path, project: str) -> str | None:
    """Return STATE.md if present, otherwise fall back to CONTEXT.md."""
    proj_dir = workspace / "projects" / project
    state = proj_dir / "STATE.md"
    if state.exists():
        return state.read_text(encoding="utf-8")
    ctx = proj_dir / "CONTEXT.md"
    if ctx.exists():
        return ctx.read_text(encoding="utf-8")
    return None


# ── Token estimator (rough, no API call) ─────────────────────────────────────
def rough_tokens(text: str) -> int:
    """~4 chars/token rule of thumb. Good enough for budget warnings."""
    return max(1, len(text) // 4)


# ── CLI helper (debug) ───────────────────────────────────────────────────────
def _cli() -> int:
    import argparse
    ap = argparse.ArgumentParser(description="Inspect skill selection for a prompt.")
    ap.add_argument("prompt", nargs="+", help="prompt text")
    ap.add_argument("--workspace", default=str(Path(__file__).resolve().parent))
    ap.add_argument("--task", action="append", default=[],
                    choices=["arch", "code", "build", "test"],
                    help="force include task_type (repeatable)")
    ap.add_argument("--project", default=None, help="include project STATE.md")
    ap.add_argument("--full", action="store_true", help="print assembled bundle")
    args = ap.parse_args()

    ws = Path(args.workspace).resolve()
    skills = discover_skills(ws / "skills")
    prompt = " ".join(args.prompt)
    detected = set(args.task) if args.task else detect_task_types(prompt)
    selected = select_skills(skills, prompt, task_types=detected)
    state = load_project_state(ws, args.project) if args.project else None

    print(f"All skills found:   {len(skills)}")
    print(f"Detected task_types: {sorted(detected) or '(none)'}")
    print(f"Selected skills ({len(selected)}):")
    for s in selected:
        print(f"  [{s.priority:5s}] {s.name:35s}  ({s.path.relative_to(ws)})")

    bundle = assemble_bundle(selected, state)
    print(f"\nBundle size:         {len(bundle):>7d} chars  (~{rough_tokens(bundle)} tok)")
    if args.full:
        print("\n" + "=" * 60 + "\n" + bundle)
    return 0


if __name__ == "__main__":
    raise SystemExit(_cli())
