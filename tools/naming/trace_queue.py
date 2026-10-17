"""Build the runtime-tracing queue (Docs\\ledgers\\TRACE_QUEUE.csv) from the low-confidence ledger rows.

Each row is one item whose meaning static reading did not establish, with what an emulator session needs to
settle it: the functions that touch it (catalog name and RAM address, for breakpoints) and, where a global
instance of the owning struct has a fixed address, the watch address of the member (for read/write watchpoints).
See Docs\\RUNTIME_TRACING.md for the workflow and priorities.

  python tools\\naming\\trace_queue.py
"""
from __future__ import annotations

import collections
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "sym_gen"))
from common import Gen, ROOT, LEDGERS  # noqa: E402

OUT = LEDGERS / "TRACE_QUEUE.csv"
PRIORITY = {"opaque": 1, "low-named": 2, "global": 2, "type": 3, "reserved": 4}


def function_addresses() -> dict[tuple[str, str], int]:
    """(overlay dir, function name) -> address, from the pinned splat symbol files and func_XXXXXXXX names."""
    out = {}
    cfg = ROOT / "resources" / "rood-reverseWin" / "config"
    for p in cfg.rglob("symbol_addrs.txt"):
        ov = p.parent.relative_to(cfg).as_posix()
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)", line)
            if m:
                out[(ov, m.group(1))] = int(m.group(2), 16)
    return out


def main() -> int:
    g = Gen("Shared")
    F = list(csv.DictReader(open(LEDGERS / "FIELD_LEDGER.csv", encoding="utf-8")))
    T = {r["id"]: r for r in csv.DictReader(open(LEDGERS / "TYPE_LEDGER.csv", encoding="utf-8"))}
    G = list(csv.DictReader(open(LEDGERS / "DATA_LEDGER.csv", encoding="utf-8")))
    cat = {(r["file"], r["name"]): r for r in csv.DictReader(open(LEDGERS / "FUNCTION_CATALOG.csv", encoding="utf-8"))}
    addrs = function_addresses()
    funcs = list(g.trees[""].ix["functions"].values())

    def fn_label(fn) -> str:
        rel = fn["file"].removeprefix("src/")
        name = (cat.get((rel, fn["name"])) or {}).get("final_name") or fn["name"]
        ov = "/".join(rel.split("/")[:2]) if not rel.startswith("SLUS") else "SLUS_010.40"
        a = addrs.get((ov, fn["name"]))
        if a is None:
            m = re.fullmatch(r"func_([0-9A-F]{8})", fn["name"])
            a = int(m.group(1), 16) if m else None
        return f"{name}@{a:08X}" if a is not None else name

    # global instances of each struct (fixed address only)
    instances = collections.defaultdict(list)
    for r in G:
        if r["type_id"] and r["address"].startswith("0x"):
            instances[r["type_id"]].append((r["final_name"] or r["name"], int(r["address"], 16), r["type"]))

    rows = []
    for f in F:
        if f["confidence"] != "low" or f["value"] != "" or f["type_id"] not in T:
            continue
        t = T[f["type_id"]]
        if "same-as:" in t["notes"]:
            continue
        kind = "reserved" if f["final_name"].startswith("reserved") else \
            "opaque" if f["final_name"].startswith("opaque") else "low-named"
        names = {t["name"], *t["aliases"].split()}
        touch = [fn_label(fn) for fn in funcs
                 if any(f"{n}.{f['name']}" in (fn.get("refs") or {}).get("field", {}) for n in names)]
        try:
            lay = g.layout(f["type_id"])
            idx = int(f["index"])
            bit = lay["fields"][idx]["offset"] if idx < len(lay["fields"]) else None
        except (KeyError, ValueError):
            bit = None
        off = f"0x{bit // 8:X}" + (f".{bit % 8}" if f["bits"] else "") if isinstance(bit, int) and bit >= 0 else ""
        watch = []
        if isinstance(bit, int) and bit >= 0:
            for gname, base, gtype in instances.get(f["type_id"], [])[:3]:
                stride = f" stride 0x{int(t['size']):X}" if "[" in gtype or "*" in gtype else ""
                if "*" in gtype:
                    watch.append(f"*{gname}+{off}")  # pointer: dereference first
                else:
                    watch.append(f"{gname}=0x{base + bit // 8:08X}{stride}")
        rows.append([f["id"], kind, PRIORITY[kind], f"{t['final_name']}.{f['final_name']}", f["name"], off,
                     f["meaning"], " ".join(touch[:6]), " ".join(watch), ""])

    for r in G:
        if r["confidence"] != "low":
            continue
        touch = [fn_label(fn) for fn in funcs if r["name"] in (fn.get("refs") or {}).get("var", {})
                 and (r["overlay"] == "SLUS_010.40" or fn["file"].removeprefix("src/").startswith(r["overlay"]))]
        watch = f"{r['final_name']}={r['address']}" if r["address"].startswith("0x") else ""
        rows.append([r["id"], "global", PRIORITY["global"], r["final_name"], r["name"], "", r["purpose"],
                     " ".join(touch[:6]), watch, r["disposition"]])

    for t in T.values():
        if t["confidence"] != "low" or "same-as:" in t["notes"]:
            continue
        inst = " ".join(f"{n}=0x{a:08X}" for n, a, _ in instances.get(t["id"], [])[:3])
        rows.append([t["id"], "type", PRIORITY["type"], t["final_name"], t["name"][:60], f"size 0x{int(t['size'] or 0):X}",
                     t["purpose"], "", inst, t["role"]])

    rows.sort(key=lambda r: (r[2], r[3]))
    with open(OUT, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(["id", "kind", "priority", "name", "original", "offset", "current_meaning", "touched_by",
                    "watch", "note"])
        w.writerows(rows)
    c = collections.Counter(r[1] for r in rows)
    print(f"{len(rows)} rows: {dict(c)}; with watch address: {sum(1 for r in rows if r[8])}; "
          f"with touching functions: {sum(1 for r in rows if r[7])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
