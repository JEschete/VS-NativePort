"""Place every code-bearing global (state/constexpr) in a file that the function catalog also uses.

A global whose final_location names a file no catalogued function lives in would become a file holding only data.
For those rows, take the catalog file of the global's users (functions in the pinned index that reference it, in
its own overlay), restricted to files under the same namespace root so the name and folder still agree; pick the
file used by most users (ties: the catalog file with most functions). Rows with no such user are reported and
left alone. Batch outputs (tools\\naming\\sym_out) are patched too, so re-applying keeps the result.

  python tools\\ledger\\reconcile_locations.py [--dry-run]
"""
from __future__ import annotations

import collections
import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
L = ROOT / "Docs" / "ledgers"
OUTS = ROOT / "tools" / "naming" / "sym_out"
CODE = {"state", "constexpr"}


def root_dir(loc: str) -> str:
    """Native\\Engine\\src\\Core, Native\\Game\\src\\Menu, Native\\Content\\src: the namespace root's folder."""
    p = loc.split("\\")
    return "\\".join(p[:4]) if p[1] in ("Engine", "Game") else "\\".join(p[:3])


def main() -> int:
    dry = "--dry-run" in sys.argv
    with (L / "FUNCTION_CATALOG.csv").open(encoding="utf-8") as stream:
        cat = list(csv.DictReader(stream))
    by_fn = {(r["file"], r["name"]): r for r in cat if r["final_location"]}
    size = collections.Counter(r["final_location"] for r in cat if r["final_location"])
    files = set(size)
    funcs = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))["functions"]
    users_of = collections.defaultdict(list)
    for f in funcs.values():
        rel = f["file"].removeprefix("src/")
        row = by_fn.get((rel, f["name"]))
        if row:
            for v in (f.get("refs") or {}).get("var", {}):
                users_of[v].append((rel, row["final_location"]))

    with (L / "DATA_LEDGER.csv").open(encoding="utf-8") as stream:
        G = list(csv.DictReader(stream))
    moves, unplaced = {}, []
    for r in G:
        loc = r["final_location"]
        if r["disposition"] not in CODE or not loc or loc in files:
            continue
        ov = r["overlay"]
        locs = [location for rel, location in users_of.get(r["name"], [])
                if (ov == "SLUS_010.40" or rel.startswith(ov + "/")
                    or (rel.count("/") == 1 and rel.startswith(ov.split("/")[0] + "/")))  # shared unit header
                and root_dir(location) == root_dir(loc)]
        if not locs:
            unplaced.append(r)
            continue
        c = collections.Counter(locs)
        best = max(c, key=lambda location: (c[location], size[location]))
        moves[r["id"]] = (loc, best)

    print(f"{len(moves)} code globals moved to a catalog file; {len(unplaced)} have no user under their root:")
    for r in unplaced:
        print(f"  {r['id']} {r['final_name']} [{r['disposition']}] {r['final_location']}")
    if dry:
        for gid, (a, b) in sorted(moves.items())[:40]:
            print(f"  {gid}: {a} -> {b}")
        return 0

    def patch(path: Path) -> int:
        raw = path.read_bytes().decode("utf-8")
        nl = "\r\n" if "\r\n" in raw else "\n"
        rows = list(csv.DictReader(raw.splitlines()))
        n = 0
        for row in rows:
            if row["id"] in moves and row["final_location"] == moves[row["id"]][0]:
                row["final_location"] = moves[row["id"]][1]
                n += 1
        if n:
            with open(path, "w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()), lineterminator=nl)
                w.writeheader()
                w.writerows(rows)
        return n

    print("ledger rows:", patch(L / "DATA_LEDGER.csv"))
    print("batch rows:", sum(patch(p) for p in OUTS.glob("*/globals.csv")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
