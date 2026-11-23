#!/usr/bin/env python3
"""Zero external side-effect check: relative links in Markdown, and MANIFEST hashes.

Run from the handoff package root:
    python3 scripts/check_handoff.py
Does not network, write, or touch machines/repos outside this directory.
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "MANIFEST.json"
MD_LINK = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
SKIP_PREFIX = ("http://", "https://", "mailto:", "ftp://")


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    errors: list[str] = []
    if not MANIFEST.is_file():
        print("FAIL: MANIFEST.json missing", file=sys.stderr)
        return 2
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    files = man.get("files") or []
    listed = {}
    for rec in files:
        rel = rec.get("path")
        if not rel or rel in ("MANIFEST.json",):
            # manifest may include itself with a placeholder; skip self-hash or handle below
            if rel == "MANIFEST.json":
                continue
        p = ROOT / rel
        if not p.is_file():
            errors.append(f"missing listed file: {rel}")
            continue
        listed[rel] = rec
        expect = rec.get("sha256")
        got = sha256(p)
        if expect and expect != got:
            errors.append(f"hash mismatch: {rel}")
        size = rec.get("bytes")
        if isinstance(size, int) and size != p.stat().st_size:
            errors.append(f"size mismatch: {rel}")

    # Every package text file should be listed, except MANIFEST.json itself
    skip_names = {".DS_Store"}
    on_disk = []
    for p in ROOT.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(ROOT).as_posix()
        if rel == "MANIFEST.json":
            continue
        if p.name in skip_names:
            continue
        if rel.endswith(".tar.gz") or rel.endswith(".zip"):
            # archives of this package if placed inside should not be required
            continue
        on_disk.append(rel)
        if rel not in listed:
            errors.append(f"on disk but not in MANIFEST: {rel}")

    for rel in listed:
        if rel not in on_disk and rel != "MANIFEST.json":
            errors.append(f"in MANIFEST but not on disk: {rel}")

    # Markdown relative links in newly written package-root docs only.
    # evidence/ copies keep original historical links to the source tree layout
    # and are not rewritten; their integrity is the SHA in MANIFEST / EVIDENCE_INDEX.
    for p in ROOT.glob("*.md"):
        text = p.read_text(encoding="utf-8", errors="replace")
        for m in MD_LINK.finditer(text):
            raw = m.group(1).strip()
            if not raw or raw.startswith("#") or raw.startswith(SKIP_PREFIX):
                continue
            path_part = raw.split("#", 1)[0].split("?", 1)[0]
            if not path_part:
                continue
            target = (p.parent / path_part).resolve()
            try:
                target.relative_to(ROOT.resolve())
            except ValueError:
                errors.append(f"link escapes package: {p.relative_to(ROOT).as_posix()} -> {raw}")
                continue
            if not target.exists():
                errors.append(f"broken link: {p.relative_to(ROOT).as_posix()} -> {raw}")

    if errors:
        print(f"FAIL: {len(errors)} issue(s)")
        for e in errors:
            print(" -", e)
        return 1
    print(f"OK: {len(on_disk)} files, links and MANIFEST hashes match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
