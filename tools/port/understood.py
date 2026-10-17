"""Marks functions `understood` (plan §"Ledger columns": `purpose` is filled, and every struct field it touches is
named in the type ledger) for everything statically reachable from a set of root functions.

  python tools\\port\\understood.py vs_title_exec@TITLE.PRG [--mark]

Reachability follows the calls libclang recorded inside each function's body (build\\port\\refs.json, function
extents from build\\port\\index.json), including calls written through macros, and catalog `dup-of` rows to the copy
that exists. Functions with no C body (INCLUDE_ASM) are reported: their purpose is checked, they touch no fields
the index can see. `--mark` writes `x` into FUNCTION_CATALOG.csv `understood` for the functions that pass; it never
clears a mark.
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LEDGERS = REPO / "Docs" / "ledgers"
PORT = REPO / "build" / "port"
GENERIC = re.compile(r"^(unk|field|pad|var|D_|_?unk)[0-9A-Fa-f_]*$|^unk[0-9A-Fa-f]+", re.I)


def rd(name: str) -> list[dict]:
    with (LEDGERS / name).open(encoding="utf-8", newline="") as fh:
        return list(csv.DictReader(fh))


def note(row: dict, key: str) -> str | None:
    m = re.search(rf"(?:^|;)\s*{key}:(\w+)", row.get("notes") or "")
    return m[1] if m else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("roots", nargs="+", help="decomp function name@overlay (e.g. vs_title_exec@TITLE.PRG)")
    ap.add_argument("--mark", action="store_true")
    ap.add_argument("--list", action="store_true", help="print every reachable function")
    a = ap.parse_args()

    cat = rd("FUNCTION_CATALOG.csv")
    by_id = {r["id"]: r for r in cat}
    fields = {r["id"]: r for r in rd("FIELD_LEDGER.csv")}
    index = json.loads((PORT / "index.json").read_text(encoding="utf-8"))["functions"]
    refs = json.loads((PORT / "refs.json").read_text(encoding="utf-8"))["files"]

    # catalog row -> body extent (file, first line, last line)
    extent = {}
    by_file_line = {(v["file"], v["line"]): v for v in index.values()}
    for r in cat:
        v = by_file_line.get(("src/" + r["file"], int(r["line"] or 0)))
        if v is not None:
            extent[r["id"]] = (v["file"], v["line"], v["end_line"])
    # refs per file sorted by line, for range queries
    per_file = {f: sorted(v) for f, v in refs.items()}

    def body_refs(fid: str) -> list:
        e = extent.get(fid)
        if e is None:
            return []
        f, lo, hi = e
        return [o for o in per_file.get(f, []) if lo <= o[0] <= hi]

    def resolve(fid: str) -> str:
        seen = set()
        while fid not in extent and fid not in seen and fid in by_id:
            seen.add(fid)
            dup = note(by_id[fid], "dup-of")
            if not dup:
                break
            fid = dup
        return fid

    roots = []
    for spec in a.roots:
        name, _, ov = spec.partition("@")
        hits = [r["id"] for r in cat if r["name"] == name and (not ov or r["overlay"] == ov)]
        if not hits:
            print(f"no catalog row for {spec}")
            return 1
        roots += hits

    reach, stack = set(), [resolve(r) for r in roots]
    while stack:
        fid = stack.pop()
        if fid in reach:
            continue
        reach.add(fid)
        for o in body_refs(fid):
            if o[3] in ("func", "func@macro") and o[4] in by_id:
                stack.append(resolve(o[4]))

    passed, failed, asm = [], [], []
    for fid in sorted(reach):
        r = by_id[fid]
        problems = []
        if not (r["purpose"] or "").strip():
            problems.append("no purpose")
        unnamed = sorted({o[4] for o in body_refs(fid) if o[3] in ("field", "field@macro")
                          and (not fields.get(o[4], {}).get("final_name") or
                               GENERIC.match(fields[o[4]]["final_name"]))})
        if unnamed:
            problems.append("unnamed fields " + ", ".join(unnamed[:6]) + ("..." if len(unnamed) > 6 else ""))
        if fid not in extent:
            asm.append(fid)
        (failed if problems else passed).append((fid, problems))

    print(f"reachable from {', '.join(a.roots)}: {len(reach)} functions; pass {len(passed)}, fail {len(failed)}; "
          f"no C body (INCLUDE_ASM) {len(asm)}")
    for fid, problems in failed:
        print(f"  FAIL {fid} {by_id[fid]['final_name']}: {'; '.join(problems)}")
    if a.list:
        for fid, _ in passed:
            print(f"  ok   {fid} {by_id[fid]['final_name']}" + ("  (no C body)" if fid in asm else ""))

    if a.mark:
        ok = {fid for fid, _ in passed}
        path = LEDGERS / "FUNCTION_CATALOG.csv"
        cols = list(cat[0].keys())
        n = 0
        for r in cat:
            if r["id"] in ok and r["understood"] != "x":
                r["understood"] = "x"
                n += 1
        with path.open("w", encoding="utf-8", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(cat)
        print(f"marked {n} newly understood")
    return 0 if not failed else 2


if __name__ == "__main__":
    sys.exit(main())
