"""Append CSV rows to a naming output file safely (always starts on a new line).

  python tools\\naming\\append_rows.py <output.csv> <rows.txt>
"""
from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    out, rows = Path(sys.argv[1]), Path(sys.argv[2]).read_text(encoding="utf-8").strip().splitlines()
    text = out.read_text(encoding="utf-8") if out.exists() else ""
    if text and not text.endswith("\n"):
        text += "\n"
    out.write_text(text + "".join(r.rstrip() + "\n" for r in rows if r.strip()), encoding="utf-8")
    print(f"{out}: appended {len([r for r in rows if r.strip()])} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
