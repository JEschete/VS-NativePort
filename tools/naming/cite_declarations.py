"""Cite declaration lines for reviewed type/field rows that lack evidence (NAMING_CONVENTIONS.md 11.1).

Run only after reviewing the names against the declarations it will cite. Types get `source:<file>:<line>` of
their declaration. Fields get the line of the member inside that declaration plus `declaration-only`, and
`high` confidence is lowered to `med`: a declaration shows layout and the original authors' name, not how the
game uses the field. Names and meanings are never changed.

  python tools\\naming\\cite_declarations.py <batch> [--dry-run]
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "resources" / "rood-reverseWin" / "src"
EVIDENCE = re.compile(r"source:[^;]*\.(c|h):\d+|DataCrystal|same-as:T\d+|view-of:G\d+")
ANON = re.compile(r"\(unnamed at (?:.*[\\/])?src[\\/](.+?):(\d+):\d+\)")


def read(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def write(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator="\n")
        w.writeheader()
        w.writerows(rows)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("batch")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    types = {r["id"]: r for r in read(ROOT / "Docs" / "ledgers" / "TYPE_LEDGER.csv")}
    fields = {r["id"]: r for r in read(ROOT / "Docs" / "ledgers" / "FIELD_LEDGER.csv")}
    cache: dict[Path, list[str]] = {}

    def lines(p: Path) -> list[str]:
        if p not in cache:
            cache[p] = p.read_text(encoding="utf-8", errors="replace").splitlines()
        return cache[p]

    def declaration(tid: str) -> tuple[Path, int, int] | None:
        """(file, first line, closing line) of the type's body."""
        t = types[tid]
        m = ANON.search(t["name"])
        if m:
            p = SRC / m.group(1).replace("\\", "/")
            p = p.resolve()
            start = int(m.group(2))
            depth = 0
            for i in range(start - 1, len(lines(p))):
                depth += lines(p)[i].count("{") - lines(p)[i].count("}")
                if depth <= 0 and "}" in lines(p)[i]:
                    return p, start, i + 1
            return None
        name = t["name"]
        for rel in t["files"].split():
            p = SRC / rel
            if not p.exists():
                continue
            text = lines(p)
            for i, source_line in enumerate(text):
                if re.search(r"\}\s*" + re.escape(name) + r"\s*;", source_line):
                    depth, j = 0, i
                    while j >= 0:
                        depth += text[j].count("}") - text[j].count("{")
                        if depth <= 0:
                            return p, j + 1, i + 1
                        j -= 1
                if re.search(r"\b(struct|union|enum)\s+" + re.escape(name) + r"\s*\{", source_line):
                    depth = 0
                    for k in range(i, len(text)):
                        depth += text[k].count("{") - text[k].count("}")
                        if depth <= 0 and "}" in text[k]:
                            return p, i + 1, k + 1
        return None

    out = ROOT / "tools" / "naming" / "sym_out" / a.batch
    unresolved = 0
    for kind in ("types", "fields"):
        path = out / f"{kind}.csv"
        if not path.exists():
            continue
        rows = read(path)
        for r in rows:
            if EVIDENCE.search(r["notes"]):
                continue
            tid = r["id"] if kind == "types" else fields[r["id"]]["type_id"]
            decl = declaration(tid)
            if decl is None:
                unresolved += 1
                print(f"{r['id']}: declaration not found; left unchanged")
                continue
            p, lo, hi = decl
            if kind == "types":
                cite = f"source:{p.name}:{lo}"
            else:
                member = fields[r["id"]]["name"]
                pat = re.compile(r"\b" + re.escape(member) + r"\b\s*(\[|:|;|=|,|$)")
                hit = next((i + 1 for i in range(lo - 1, hi) if pat.search(lines(p)[i])), None)
                if hit is None:
                    unresolved += 1
                    print(f"{r['id']} {member}: member not found in {p.name}:{lo}-{hi}; left unchanged")
                    continue
                cite = f"declaration-only;source:{p.name}:{hit}"
                if r["confidence"] == "high":
                    r["confidence"] = "med"
            r["notes"] = ";".join(x for x in [r["notes"], cite] if x)
            print(f"{r['id']}: {cite}")
        if not a.dry_run:
            write(path, rows)
    return 1 if unresolved else 0


if __name__ == "__main__":
    sys.exit(main())
