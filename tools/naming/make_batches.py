"""Split FUNCTION_CATALOG.csv into naming batches for the Phase 1a naming pass (plan §5 step 3).

Writes tools\\naming\\batches\\<batch>.csv. Each row has the catalog context plus caller/callee *names* (not just
counts), so an agent can name a function from its neighbourhood. Effect template instantiations (`kind` template:*) are
named mechanically by merge_names.py and are not batched.

Usage: python tools\\naming\\make_batches.py [--max 180]
"""
from __future__ import annotations

import argparse
import collections
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ledger"))
import build_ledgers as bl  # noqa: E402

CATALOG = ROOT / "Docs" / "ledgers" / "FUNCTION_CATALOG.csv"
OUT = ROOT / "tools" / "naming" / "batches"

COLS = ["id", "file", "line", "name", "kind", "lines", "proposed_system", "sdk_apis", "callers", "callees"]


def batch_key(row: dict) -> str:
    unit = row["unit"]
    if unit in ("TITLE", "ENDING", "GIM", "renderTextures.h"):
        return "TITLE_ENDING"
    if unit == "EFFECT":
        return "EFFECT"
    return unit


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--max", type=int, default=180)
    args = ap.parse_args()

    funcs, *_ = bl.scan(bl.load_sdk())
    game = [f for f in funcs if not f["sdk"]]
    names = {f["name"] for f in game}
    callees: dict[tuple[str, str], list[str]] = {}
    callers: dict[str, list[tuple[str, str]]] = collections.defaultdict(list)
    for f in game:
        cs = sorted({c for c in bl.CALL.findall(f["body"]) if c in names and c != f["name"]})
        callees[(f["file"], f["name"])] = cs
        for c in cs:
            callers[c].append((f["overlay"], f["name"]))

    rows = [r for r in csv.DictReader(CATALOG.open(encoding="utf-8"))
            if r["copy"] == "yes" and not r["kind"].startswith("template:")]

    def ctx(r: dict) -> tuple[str, str]:
        ce = callees.get((r["file"], r["name"]), [])
        # Same-overlay callers first, then shared-engine (SLUS) callers.
        cr = sorted(callers.get(r["name"], []), key=lambda x: (x[0] != r["overlay"], x[0], x[1]))
        crs = [n if o == r["overlay"] else f"{o}:{n}" for o, n in cr]
        trim = lambda xs, n: " ".join(xs[:n]) + (f" (+{len(xs) - n})" if len(xs) > n else "")
        return trim(crs, 10), trim(ce, 14)

    groups: dict[str, list[dict]] = collections.defaultdict(list)
    for r in rows:
        groups[batch_key(r)].append(r)

    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.csv"):
        old.unlink()
    manifest = []
    for key, grp in sorted(groups.items()):
        grp.sort(key=lambda r: (r["file"], int(r["line"] or 0), r["name"]))
        # Pack whole files together; split oversized files into contiguous line-ordered chunks.
        by_file: dict[str, list[dict]] = collections.defaultdict(list)
        for r in grp:
            by_file[r["file"]].append(r)
        chunks: list[list[dict]] = []
        cur: list[dict] = []
        for _file, frs in by_file.items():
            if len(frs) > args.max:
                if cur:
                    chunks.append(cur)
                    cur = []
                n = -(-len(frs) // args.max)
                size = -(-len(frs) // n)
                chunks += [frs[i:i + size] for i in range(0, len(frs), size)]
                continue
            if len(cur) + len(frs) > args.max:
                chunks.append(cur)
                cur = []
            cur += frs
        if cur:
            chunks.append(cur)
        for i, chunk in enumerate(chunks, 1):
            name = f"{key}_{i:02d}"
            with (OUT / f"{name}.csv").open("w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=COLS)
                w.writeheader()
                for r in chunk:
                    cr, ce = ctx(r)
                    w.writerow({**{c: r[c] for c in COLS if c in r}, "callers": cr, "callees": ce})
            files = sorted({r["file"] for r in chunk})
            manifest.append((name, len(chunk), sum(r["generic"] == "1" for r in chunk), files))
    with (OUT / "MANIFEST.md").open("w", encoding="utf-8") as fh:
        fh.write("| batch | rows | generic | files |\n|---|---:|---:|---|\n")
        for name, n, g, files in manifest:
            fh.write(f"| {name} | {n} | {g} | {', '.join(files[:6])}{' …' if len(files) > 6 else ''} |\n")
    for name, n, g, _ in manifest:
        print(f"{name:18} {n:4} rows {g:4} generic")
    print(f"{len(manifest)} batches, {sum(m[1] for m in manifest)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
