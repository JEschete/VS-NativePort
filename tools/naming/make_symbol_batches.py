"""Split DATA/TYPE/FIELD ledger rows into naming batches (Phase 1a symbol pass, NAMING_CONVENTIONS.md Part 2).

Writes tools\\naming\\sym_batches\\<batch>\\{globals,types,fields}.csv + context.md.
Rows are grouped by proposed system so one agent sees a system's globals and types together; a type always travels
with all its fields. Only rows without a final_name are batched, so re-running after a merge batches what is left.
"""
from __future__ import annotations

import csv
import json
import shutil
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGERS = ROOT / "Docs" / "ledgers"
OUT = ROOT / "tools" / "naming" / "sym_batches"
CTX = ROOT / "build" / "port" / "symbol_context.json"
BUDGET = 420          # weight per batch: global = 1.6, type = 2, field = 0.4
MAX_USERS = 12

GROUP = {
    "Audio": "Audio", "Core": "Engine", "Input": "Engine", "Video": "Engine", "Graphics": "Engine",
    "Content": "Engine", "Save": "Engine", "Game.Boot": "Engine", "Game.State": "Engine",
    "Game.Items": "Game.Actors", "": "Shared",
}


def group_of(system: str) -> str:
    for k in (system, system.split(".")[0]):
        if k in GROUP:
            return GROUP[k]
    return system


def read(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def write(path: Path, rows: list[dict], cols: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)


G_COLS = ["id", "key", "name", "overlay", "linkage", "segment", "address", "size", "type", "type_id", "has_pointers",
          "init", "instances", "files", "used_by", "proposed_system", "proposed_disposition"]
T_COLS = ["id", "key", "kind", "name", "aliases", "size", "fields", "generic_fields", "layouts", "files",
          "used_by_funcs", "used_by_globals", "proposed_system"]
F_COLS = ["id", "type_id", "type_name", "index", "name", "type", "bits", "value", "hints"]


def main() -> int:
    data = [r for r in read(LEDGERS / "DATA_LEDGER.csv") if not r["final_name"]]
    types = read(LEDGERS / "TYPE_LEDGER.csv")
    fields = read(LEDGERS / "FIELD_LEDGER.csv")
    ctx = json.loads(CTX.read_text(encoding="utf-8"))
    f_by_type: dict[str, list[dict]] = defaultdict(list)
    for f in fields:
        f_by_type[f["type_id"]].append(f)
    todo_types = [t for t in types if not t["final_name"]]

    groups: dict[str, dict[str, list]] = defaultdict(lambda: {"g": [], "t": []})
    for r in data:
        groups[group_of(r["proposed_system"])]["g"].append(r)
    for t in todo_types:
        groups[group_of(t["proposed_system"])]["t"].append(t)

    if OUT.exists():
        shutil.rmtree(OUT)
    batches = []
    for gname in sorted(groups):
        gs = sorted(groups[gname]["g"], key=lambda r: (r["overlay"], r["address"] or "~", r["key"]))
        ts = sorted(groups[gname]["t"], key=lambda r: r["name"])
        # Chunk: types first (each with its fields), then globals, closing a batch at the budget.
        chunks, cur, w = [], {"g": [], "t": []}, 0.0
        for t in ts:
            tw = 2 + 0.4 * len(f_by_type[t["id"]])
            if cur["t"] and w + tw > BUDGET:
                chunks.append(cur); cur, w = {"g": [], "t": []}, 0.0
            cur["t"].append(t); w += tw
        for g in gs:
            if (cur["g"] or cur["t"]) and w + 1.6 > BUDGET:
                chunks.append(cur); cur, w = {"g": [], "t": []}, 0.0
            cur["g"].append(g); w += 1.6
        if cur["g"] or cur["t"]:
            if chunks and w < BUDGET * 0.1:  # fold a tiny tail into the previous batch
                chunks[-1]["g"] += cur["g"]; chunks[-1]["t"] += cur["t"]
            else:
                chunks.append(cur)
        for n, c in enumerate(chunks, 1):
            name = f"{gname.replace('.', '_')}_{n:02d}" if len(chunks) > 1 else gname.replace(".", "_")
            d = OUT / name
            d.mkdir(parents=True)
            fs = [f for t in c["t"] for f in f_by_type[t["id"]]]
            write(d / "globals.csv", c["g"], G_COLS)
            write(d / "types.csv", c["t"], T_COLS)
            write(d / "fields.csv", fs, F_COLS)
            lines = [f"# {name}: who uses what", "",
                     "Function names are `final name (decomp file:line original name)`.", ""]
            if c["g"]:
                lines += ["## Globals", ""]
                for g in c["g"]:
                    users = ctx["globals"].get(g["id"], [])
                    more = f" … +{len(users) - MAX_USERS} more" if len(users) > MAX_USERS else ""
                    lines.append(f"- **{g['id']}** `{g['name']}` `{g['type']}` ({g['linkage']}, {g['segment']}): "
                                 + ("; ".join(users[:MAX_USERS]) + more if users else "*no function references*"))
                lines.append("")
            if c["t"]:
                lines += ["## Types", ""]
                for t in c["t"]:
                    users = ctx["types"].get(t["id"], [])
                    more = f" … +{len(users) - MAX_USERS} more" if len(users) > MAX_USERS else ""
                    lines.append(f"- **{t['id']}** `{t['key']}`: "
                                 + ("; ".join(users[:MAX_USERS]) + more if users else "*no function references*"))
            (d / "context.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
            batches.append((name, len(c["g"]), len(c["t"]), len(fs)))

    for b in batches:
        print(f"{b[0]:22} globals {b[1]:4} types {b[2]:4} fields {b[3]:5}")
    print(f"{len(batches)} batches -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
