"""Copy completeness check: decomp-style names left in Native\\ code (outside comments and strings).

Every identifier the copy moves is renamed by position from refs.json. Occurrences libclang could not record
(expressions in the decomp's error translation units, text it never parsed) keep their decomp spelling; this lists
them so the copy tool or copy_fixups.py can handle each one.

  python tools\\port\\check_leftovers.py
"""
from __future__ import annotations

import collections
import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CODE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"', re.S)
DECOMP_NAME = re.compile(r"\b(func_[0-9A-Fa-f]{8}|D_[0-9A-Fa-f]{8}\w*|vs_[a-z]\w*|_[a-z]\w+_t\b|\w+_t[0-9]?\b)")
STANDARD = {"size_t", "uintptr_t", "intptr_t", "ptrdiff_t", "wchar_t"} | {
    f"{s}int{n}_t" for s in ("", "u") for n in (8, 16, 32, 64)}


def main() -> int:
    files = list(json.loads((REPO / "tools" / "port" / "copy_manifest.json").read_text(encoding="utf-8")))
    count: collections.Counter = collections.Counter()
    where: dict[str, set] = collections.defaultdict(set)
    for p in files:
        text = CODE.sub(" ", (REPO / p).read_text(encoding="utf-8", errors="replace"))
        for m in DECOMP_NAME.finditer(text):
            if m[1] not in STANDARD:
                count[m[1]] += 1
                where[m[1]].add(Path(p).name)
    for name, n in count.most_common():
        print(f"{n:5} {name:40} {', '.join(sorted(where[name])[:4])}")
    print(f"{len(count)} names, {sum(count.values())} occurrences")
    return 1 if count else 0


if __name__ == "__main__":
    raise SystemExit(main())
