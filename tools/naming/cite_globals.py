"""Add source citations to reviewed global rows that lack evidence (NAMING_CONVENTIONS.md 11.1).

Only run this after the rows' names have been reviewed against the lines it will cite. For each row without
evidence it resolves the storage identity's own definition (file-static/local-static scope from the decomp index)
and its first use inside the owning function (local statics) or file, then appends
`source:<file>:<line>` tokens. It never changes names, purposes or confidence.

  python tools\\naming\\cite_globals.py <batch> [--dry-run]
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / "resources" / "rood-reverseWin"
EVIDENCE = re.compile(r"source:[^;]*\.(c|h):\d+|DataCrystal|same-as:T\d+|view-of:G\d+")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("batch")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    index = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))
    funcs: dict[tuple[str, str], tuple[int, int]] = {}
    for fn in index["functions"].values():
        funcs[(fn["file"], fn["name"])] = (fn["line"], fn["end_line"])
    with (ROOT / "Docs" / "ledgers" / "DATA_LEDGER.csv").open(encoding="utf-8") as stream:
        data = {r["id"]: r for r in csv.DictReader(stream)}
    context = json.loads((ROOT / "build" / "port" / "symbol_context.json").read_text(encoding="utf-8"))["globals"]
    path = ROOT / "tools" / "naming" / "sym_out" / a.batch / "globals.csv"
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    cols = list(rows[0])
    cache: dict[str, list[str]] = {}

    def lines(rel: str) -> list[str]:
        if rel not in cache:
            cache[rel] = (REF / rel).read_text(encoding="utf-8", errors="replace").splitlines()
        return cache[rel]

    unresolved = 0
    for r in rows:
        if EVIDENCE.search(r["notes"]):
            continue
        d = data[r["id"]]
        name, where = d["name"], d["key"].split("@", 1)[1]
        path_part, _, scope = where.partition(":") if where.startswith("src/") else ("", "", "")
        entry = index["globals"].get(name, {})
        cands = entry.get("defs", []) + entry.get("decls", [])
        pick = None
        for c in cands:
            if path_part and c["file"] != path_part:
                continue
            if scope and c.get("scope") != scope:
                continue
            if not path_part and d["overlay"] != "SLUS_010.40" and d["overlay"] not in c["file"]:
                continue
            pick = c
            break
        if pick is None:
            unresolved += 1
            print(f"{r['id']} {name}: definition not resolved; left unchanged")
            continue
        rel, ln = pick["file"], pick["line"]
        lo, hi = funcs.get((rel, scope), (1, len(lines(rel)))) if scope else (1, len(lines(rel)))
        if scope:
            lo = ln + 1  # a local static is only used after its declaration
        pattern = re.compile(r"\b" + re.escape(name) + r"\b")

        def first_use(file: str, start: int, end: int, *, declaration_file=rel,
                      declaration_line=ln, name_pattern=pattern) -> int | None:
            return next((i + 1 for i, source_line in enumerate(lines(file)) if start <= i + 1 <= end and not (
                file == declaration_file and i + 1 == declaration_line) and name_pattern.search(source_line)
                and not source_line.lstrip().startswith("extern")
                and not re.match(r"\s*(enum|struct|typedef)\b", source_line)), None)

        use_file, use = rel, first_use(rel, lo, hi)
        if use is None and not scope:
            for label in context.get(r["id"], []):
                m = re.search(r"\(([^:()]+):(\d+) (\w+)\)$", label)
                if not m:
                    continue
                other = f"src/{m.group(1)}"
                start, end = funcs.get((other, m.group(3)), (int(m.group(2)), int(m.group(2))))
                if (REF / other).exists():
                    use = first_use(other, start, end)
                    if use:
                        use_file = other
                        break
        cites = [f"source:{Path(rel).name}:{ln}"] + ([f"source:{Path(use_file).name}:{use}"] if use else [])
        r["notes"] = ";".join(x for x in [r["notes"], *cites] if x)
        print(f"{r['id']} {name}: {' '.join(cites)}")
    if not a.dry_run:
        with path.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
    return 1 if unresolved else 0


if __name__ == "__main__":
    sys.exit(main())
