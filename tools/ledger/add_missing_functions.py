"""Append catalog rows for functions the source scanner of build_ledgers.py missed.

Two kinds are found:
- C functions: the pinned libclang index (build\\port\\index.json) lists every function defined in a .c file;
  build_ledgers.py finds functions with a regex and loses some.
- Inline-assembly functions: `__asm__("glabel name; ...")` strings inside .c files (and `alabel` alternate entry
  points into them), which neither scanner sees. Their body runs to the matching `endlabel` (or the next label).

Re-running build_ledgers.py renumbers every F id, so the missing functions are appended with new ids after the last
one, with columns from build_ledgers' own helpers. Also writes the naming batch input
tools\\naming\\batches\\<batch>.csv (never overwrites an existing batch). Idempotent: functions already in the catalog
are skipped. See Docs\\NAMING_CONVENTIONS.md, "Function catalog gap".

  python tools\\ledger\\add_missing_functions.py --batch CATALOG_GAP_ASM [--dry-run]
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_ledgers as bl  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CATALOG = bl.OUT / "FUNCTION_CATALOG.csv"
BATCHES = ROOT / "tools" / "naming" / "batches"
LABEL = re.compile(r'"\s*(glabel|alabel|endlabel)\s+(\w+)\s*;?|"\s*(jumpFunc)\s+(\w+)\s*,\s*\w+\s*;')
ASM_CALL = re.compile(r"\b(?:jal|j)\s+([A-Za-z_]\w*)\b")


def inline_asm_entries(have: set) -> list[dict]:
    out = []
    for p in sorted(bl.SRC.rglob("*.c")):
        rel = p.relative_to(bl.SRC).as_posix()
        if bl.is_sdk_path(Path(rel)):
            continue
        lines = bl.read(p).splitlines()
        labels = []
        for i, line in enumerate(lines):
            for m in LABEL.finditer(line):
                if m[3]:  # `jumpFunc name, target;` expands to a whole glabel..endlabel function on one line
                    labels += [(i + 1, "glabel", m[4]), (i + 1, "endlabel", m[4])]
                else:
                    labels.append((i + 1, m[1], m[2]))
        for k, (line, kind, name) in enumerate(labels):
            if kind == "endlabel" or (rel, name) in have:
                continue
            end = next((ln for ln, kd, nm in labels[k + 1:] if kd != "alabel" and (kd != "endlabel" or nm == name)
                        or (kind == "alabel" and kd == "endlabel")), line)
            out.append({"file": rel, "line": line, "end_line": end, "name": name,
                        "kind": "inline-asm" if kind == "glabel" else "inline-asm-entry",
                        "body": "\n".join(lines[line - 1:end])})
    return out


def asm_file_entries(have: set) -> list[dict]:
    """`alabel` alternate entry points inside .s files that C code references (calls or takes the address of).
    Internal jump labels that no C code names are not functions and are skipped."""
    ctext = "\n".join(bl.read(p) for p in bl.SRC.rglob("*.[ch]") if not bl.is_sdk_path(p.relative_to(bl.SRC)))
    out = []
    for p in sorted(bl.SRC.rglob("*.s")):
        rel = p.relative_to(bl.SRC).as_posix()
        if bl.is_sdk_path(Path(rel)):
            continue
        lines = bl.read(p).splitlines()
        for i, line in enumerate(lines):
            m = re.match(r"\s*alabel\s+(\w+)", line)
            if not m or (rel, m[1]) in have or not re.search(r"\b" + m[1] + r"\b", ctext):
                continue
            end = next((j for j in range(i + 1, len(lines)) if re.match(r"\s*(glabel|endlabel|alabel)\b", lines[j])),
                       len(lines) - 1)
            out.append({"file": rel, "line": i + 1, "end_line": end + 1, "name": m[1], "kind": "asm-entry",
                        "body": "\n".join(lines[i:end + 1])})
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--batch", required=True)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    batch_path = BATCHES / f"{a.batch}.csv"
    raw = CATALOG.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in raw else "\n"
    rows = list(csv.DictReader(raw.splitlines()))
    cols = list(rows[0].keys())
    have = {(r["file"], r["name"]) for r in rows}
    index = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))["functions"]
    texts: dict[str, list[str]] = {}

    def c_body(f) -> str:
        rel = f["file"].removeprefix("src/")
        if rel not in texts:
            texts[rel] = bl.strip_comments(bl.read(bl.SRC / rel)).splitlines()
        return "\n".join(texts[rel][f["line"] - 1:f["end_line"]])

    missing = [{"file": f["file"].removeprefix("src/"), "line": f["line"], "end_line": f["end_line"],
                "name": f["name"], "kind": "C", "body": c_body(f)}
               for f in index.values() if f["file"].endswith(".c") and not f["file"].startswith("src/EFFECT/")
               and (f["file"].removeprefix("src/"), f["name"]) not in have]
    missing += inline_asm_entries(have)
    missing += asm_file_entries(have)
    missing.sort(key=lambda f: (f["file"], f["line"]))
    if not missing:
        print("no missing functions")
        return 0
    if batch_path.exists() and not a.dry_run:
        sys.exit(f"{batch_path.relative_to(ROOT)} exists; choose another --batch name")

    sdk = bl.load_sdk()
    funcs, *_ = bl.scan(sdk)
    game_names = {f["name"] for f in funcs if not f["sdk"]} | {f["name"] for f in missing}
    bodies = [(f["name"], f["body"]) for f in funcs if f["body"]] + [(f["name"], f["body"]) for f in missing]

    def calls(body: str) -> set[str]:
        return set(bl.CALL.findall(body)) | set(ASM_CALL.findall(body))

    caller_names = collections.defaultdict(set)
    for name, body in bodies:
        for c in calls(body):
            if c != name:
                caller_names[c].add(name)

    next_id = max(int(r["id"][1:]) for r in rows) + 1
    new, batch = [], []
    for f in missing:
        rel, body = f["file"], f["body"]
        unit, overlay = bl.unit_of(Path(rel))
        callees, apis, api_names = set(), {}, set()
        for c in calls(body):
            if c == f["name"] or c in bl.KEYWORDS:
                continue
            if c in game_names:
                callees.add(c)
            elif c in sdk:
                lib = "libpress" if bl.VIDEO_NAMES.match(c) else sdk[c]["lib"]
                apis[lib] = apis.get(lib, 0) + 1
                api_names.add(c)
        system, basis = bl.classify(f["name"], unit, rel, collections.Counter(apis))
        callers = sorted(caller_names.get(f["name"], set()))
        row = {c: "" for c in cols}
        row.update({
            "id": f"F{next_id:05d}", "overlay": overlay, "unit": unit, "file": rel, "line": f["line"],
            "name": f["name"], "kind": f["kind"], "generic": int(f["name"].startswith("func_")),
            "lines": f["end_line"] - f["line"] + 1, "proposed_system": system, "basis": basis, "copy": "yes",
            "callers": len(callers), "callees": len(callees), "sdk_apis": " ".join(sorted(api_names)),
            **{k: len(rx.findall(body)) for k, rx in bl.HAZARDS.items()},
            "notes": "appended by add_missing_functions.py (missed by the build_ledgers scanner)",
        })
        new.append(row)
        batch.append({"id": row["id"], "file": rel, "line": f["line"], "name": f["name"], "kind": f["kind"],
                      "lines": row["lines"], "proposed_system": system, "sdk_apis": row["sdk_apis"],
                      "callers": " ".join(callers), "callees": " ".join(sorted(callees))})
        next_id += 1
    print(f"{len(new)} rows: {new[0]['id']}..{new[-1]['id']}")
    if a.dry_run:
        for r in batch:
            print(r["id"], r["file"], r["line"], r["name"], r["kind"], r["proposed_system"], "| callers:", r["callers"][:70])
        return 0
    with CATALOG.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=cols, lineterminator=nl)
        w.writeheader()
        w.writerows(rows + new)
    with batch_path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(batch[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(batch)
    print(f"appended to {CATALOG.name}; wrote {batch_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
