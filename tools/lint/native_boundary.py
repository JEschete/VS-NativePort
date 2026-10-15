"""Enforce the native-port hard rules (plan §2) over Native\\.

Fails (exit 1) when any source under Native\\ contains:
  - an include of the decomp tree or a PS1 SDK header (libgpu.h, libgte.h, ...)
  - a raw PS1 RAM/kseg address (0x80xxxxxx / 0xA0xxxxxx) or scratchpad address (0x1F80xxxx)
  - getScratchAddr / D_80xxxxxx / func_80xxxxxx symbols (must be renamed by the copy tool)

Usage: python tools\\lint\\native_boundary.py [--summary]
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "Native"
EXTS = {".c", ".h", ".cpp", ".hpp", ".inl"}

SDK_HEADERS = ("libgpu", "libgte", "libgs", "libetc", "libapi", "libcd", "libds", "libspu", "libsnd", "libpress",
               "libcard", "libmcrd", "libpad", "libsio", "kernel.h", "r3000.h", "asm.h", "inline_c.h", "gtemac")

RULES: list[tuple[str, re.Pattern[str]]] = [
    ("decomp-include", re.compile(r'#\s*include\s*[<"][^>"]*(rood-reverse|resources[/\\])', re.I)),
    ("sdk-include", re.compile(r'#\s*include\s*[<"][^>"]*(' + "|".join(re.escape(h) for h in SDK_HEADERS) + r')',
                               re.I)),
    ("ps1-address", re.compile(r'\b0x(80|A0)[0-9A-Fa-f]{6}\b')),
    ("scratchpad", re.compile(r'\b0x1F80[0-9A-Fa-f]{4}\b|getScratchAddr')),
    ("raw-symbol", re.compile(r'\b(D|func)_80[0-9A-Fa-f]{6}\b')),
]


def strip_comments(text: str) -> str:
    # Provenance comments may cite original addresses; only code is checked.
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def main() -> int:
    summary = "--summary" in sys.argv
    violations: list[tuple[str, int, str, str]] = []
    files = [p for p in NATIVE.rglob("*") if p.suffix.lower() in EXTS and p.is_file()]
    for path in files:
        code = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
        for lineno, line in enumerate(code.splitlines(), 1):
            for rule, rx in RULES:
                if rx.search(line):
                    violations.append((str(path.relative_to(ROOT)), lineno, rule, line.strip()[:120]))
    if summary or len(violations) > 50:
        counts: dict[str, int] = {}
        for _, _, rule, _ in violations:
            counts[rule] = counts.get(rule, 0) + 1
        for rule, n in sorted(counts.items()):
            print(f"{rule:16} {n}")
    if not summary:
        for f, n, rule, text in violations[:50]:
            print(f"{f}:{n}: [{rule}] {text}")
    print(f"native_boundary: {len(files)} files, {len(violations)} violations")
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
