"""Append catalog rows for C functions the source scanner of build_ledgers.py missed.

The pinned libclang index (build\\port\\index.json) lists every function defined in a .c file; build_ledgers.py
finds functions with a regex and loses some (Docs\\NAMING_CONVENTIONS.md, "Function catalog gap"). Re-running
build_ledgers.py renumbers every F id, so the missing functions are appended with new ids after the last one.
Columns come from build_ledgers' own helpers over the index's line range. Also writes the naming batch input
tools\\naming\\batches\\CATALOG_GAP.csv.  Idempotent: functions already in the catalog are skipped.

  python tools\\ledger\\add_missing_functions.py [--dry-run]
"""
from __future__ import annotations

import collections
import csv
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_ledgers as bl  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CATALOG = bl.OUT / "FUNCTION_CATALOG.csv"
BATCH = ROOT / "tools" / "naming" / "batches" / "CATALOG_GAP.csv"


def main() -> int:
    dry = "--dry-run" in sys.argv
    with CATALOG.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    cols = list(rows[0].keys())
    have = {(r["file"], r["name"]) for r in rows}
    index = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))["functions"]
    missing = sorted((f for f in index.values() if f["file"].endswith(".c") and not f["file"].startswith("src/EFFECT/")
                      and (f["file"].removeprefix("src/"), f["name"]) not in have), key=lambda f: (f["file"], f["line"]))
    if not missing:
        print("no missing functions")
        return 0

    sdk = bl.load_sdk()
    funcs, *_ = bl.scan(sdk)
    game_names = {f["name"] for f in funcs if not f["sdk"]} | {f["name"] for f in missing}
    texts: dict[str, list[str]] = {}

    def body_of(f) -> str:
        rel = f["file"].removeprefix("src/")
        if rel not in texts:
            texts[rel] = bl.strip_comments(bl.read(bl.SRC / rel)).splitlines()
        return "\n".join(texts[rel][f["line"] - 1:f["end_line"]])

    bodies = [(f["name"], f["body"]) for f in funcs if f["body"]] + [(f["name"], body_of(f)) for f in missing]
    callers = {f["name"]: 0 for f in missing}
    for name, body in bodies:
        for c in set(bl.CALL.findall(body)):
            if c in callers and c != name:
                callers[c] += 1

    next_id = max(int(r["id"][1:]) for r in rows) + 1
    new, batch = [], []
    for f in missing:
        rel = f["file"].removeprefix("src/")
        unit, overlay = bl.unit_of(Path(rel))
        body = body_of(f)
        callees, apis, api_names = set(), {}, set()
        for c in bl.CALL.findall(body):
            if c == f["name"] or c in bl.KEYWORDS:
                continue
            if c in game_names:
                callees.add(c)
            elif c in sdk:
                lib = "libpress" if bl.VIDEO_NAMES.match(c) else sdk[c]["lib"]
                apis[lib] = apis.get(lib, 0) + 1
                api_names.add(c)
        system, basis = bl.classify(f["name"], unit, rel, collections.Counter(apis))
        row = {c: "" for c in cols}
        row.update({
            "id": f"F{next_id:05d}", "overlay": overlay, "unit": unit, "file": rel, "line": f["line"],
            "name": f["name"], "kind": "C", "generic": int(f["name"].startswith("func_")),
            "lines": f["end_line"] - f["line"] + 1, "proposed_system": system, "basis": basis, "copy": "yes",
            "callers": callers[f["name"]], "callees": len(callees), "sdk_apis": " ".join(sorted(api_names)),
            **{k: len(rx.findall(body)) for k, rx in bl.HAZARDS.items()},
            "notes": "appended by add_missing_functions.py (missed by the build_ledgers scanner)",
        })
        new.append(row)
        batch.append({"id": row["id"], "file": rel, "line": f["line"], "name": f["name"], "kind": "C",
                      "lines": row["lines"], "proposed_system": system, "sdk_apis": row["sdk_apis"],
                      "callers": " ".join(sorted(n for n, b in bodies if n != f["name"] and f["name"] in set(bl.CALL.findall(b)))),
                      "callees": " ".join(sorted(callees))})
        next_id += 1
    print(f"{len(new)} rows: {new[0]['id']}..{new[-1]['id']}")
    if dry:
        for r in batch:
            print(r["id"], r["file"], r["line"], r["name"], r["proposed_system"], "| callers:", r["callers"][:80])
        return 0
    with CATALOG.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=cols, lineterminator="\n")
        w.writeheader()
        w.writerows(rows + new)
    with BATCH.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(batch[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(batch)
    print(f"appended to {CATALOG.name}; wrote {BATCH.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
