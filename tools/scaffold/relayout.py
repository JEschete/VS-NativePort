"""One-off: move every recorded location to the hybrid layout (owner decision 2026-10-17).

  Native\\<System>\\{src,include,Legacy}\\...  ->  Native\\Engine\\{src,include,Legacy}\\<System>\\...
  for Core, Platform, Graphics->Render, Audio, Input, Video->Movie, Save. Content and Game keep their paths.

Rewrites ledgers, naming batch outputs, generators and docs in place (bytes and line endings preserved apart from
the paths). Idempotent: already-moved paths no longer match.

  python tools\\scaffold\\relayout.py [--dry-run]
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SYSTEMS = {"Core": "Core", "Platform": "Platform", "Graphics": "Render", "Audio": "Audio", "Input": "Input",
           "Video": "Movie", "Save": "Save"}
# separator is one backslash, an escaped backslash (Python source) or a slash; it is reused for the whole path
PATH = re.compile(r"Native(\\\\|\\|/)(" + "|".join(SYSTEMS) + r")\1(src|include|Legacy)(?=\1)")
TARGETS = [
    "Docs/ledgers/*.csv", "Docs/ledgers/*.md", "Docs/*.md",
    "tools/naming/out/*.csv", "tools/naming/sym_out/*/*.csv",
    "tools/naming/*.py", "tools/naming/sym_gen/*.py", "tools/ledger/*.py", "tools/lint/*.py", "tools/port/*.py",
]


def remap(text: str) -> tuple[str, int]:
    return PATH.subn(lambda m: f"Native{m[1]}Engine{m[1]}{m[3]}{m[1]}{SYSTEMS[m[2]]}", text)


def main() -> int:
    dry = "--dry-run" in sys.argv
    total = 0
    for pattern in TARGETS:
        for p in sorted(ROOT.glob(pattern)):
            raw = p.read_bytes()
            text = raw.decode("utf-8")
            new, n = remap(text)
            if n:
                total += n
                print(f"{n:6} {p.relative_to(ROOT)}")
                if not dry:
                    p.write_bytes(new.encode("utf-8"))
    print(f"{total} paths {'would be ' if dry else ''}rewritten")
    return 0


if __name__ == "__main__":
    sys.exit(main())
