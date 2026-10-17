"""Validate symbol-naming outputs and apply them to DATA/TYPE/FIELD ledgers (NAMING_CONVENTIONS.md Part 2).

  python tools\\naming\\merge_symbols.py --check Game_Title   validate one batch output (agents run this)
  python tools\\naming\\merge_symbols.py --check-all          validate every output + cross-batch uniqueness
  python tools\\naming\\merge_symbols.py --apply              validate all, then write the ledgers' naming columns
  python tools\\naming\\merge_symbols.py --apply-batch Game_Title
                                                            validate and apply one complete batch
Exit code 1 on any error.
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGERS = ROOT / "Docs" / "ledgers"
BATCHES = ROOT / "tools" / "naming" / "sym_batches"
OUTS = ROOT / "tools" / "naming" / "sym_out"
CATALOG = LEDGERS / "FUNCTION_CATALOG.csv"

HEADERS = {
    "globals": ["id", "disposition", "final_name", "final_location", "purpose", "confidence", "notes"],
    "types": ["id", "role", "final_name", "final_location", "purpose", "confidence", "notes"],
    "fields": ["id", "final_name", "meaning", "confidence", "notes"],
}
LEDGER = {"globals": "DATA_LEDGER.csv", "types": "TYPE_LEDGER.csv", "fields": "FIELD_LEDGER.csv"}

# Hybrid layout (tools\scaffold\gen_solution.py): engine systems live in Native\Engine\{src,include}\<Root>\.
ENGINE_ROOTS = {"Core", "Platform", "Render", "Audio", "Input", "Movie", "Save"}
GAME_MODULES = {"Boot", "Title", "Field", "Battle", "Actors", "Items", "Menu", "Event", "Effects", "Ending", "State"}
DISPOSITIONS = {"catalog", "constexpr", "state", "asset", "drop"}
ROLES = {"Def", "Instance", "State", "OnDisc", "Packet", "Enum", "Value", "Misc"}
CONF = {"high", "med", "low"}
# A name is accepted only with a pointer to the code (or format documentation) it was derived from.
EVIDENCE = re.compile(r"source:[^;]*\.(c|h):\d+|DataCrystal|same-as:T\d+|view-of:G\d+")

NS = r"[A-Z][A-Za-z0-9]*(?:::[A-Z][A-Za-z0-9]*){0,2}"
GLOBAL_RX = re.compile(rf"^{NS}::(?:[a-z][A-Za-z0-9]*|k[A-Z][A-Za-z0-9]*)$")
TYPE_RX = re.compile(rf"^{NS}::[A-Z][A-Za-z0-9]*$")
FIELD_RX = re.compile(r"^[a-z][A-Za-z0-9]*$")
ENUMERATOR_RX = re.compile(r"^[A-Z][A-Za-z0-9]*$")
FORBIDDEN = re.compile(r"(^|::|[a-z])(Unk|Unknown|Temp)([A-Z0-9]|$)|^unk|::unk|[0-9A-Fa-f]{8}|"
                       r"(^|::)(vs|Vs|g|s)_|::(D|B)_|(^|::)(data|buf|table)$")
SRC_LOC = re.compile(r"^Native\\(?P<proj>[A-Za-z]+)\\src\\(?:[A-Z][A-Za-z0-9]*\\){0,3}[A-Z][A-Za-z0-9]*\.c$")
HDR_LOC = re.compile(r"^Native\\(?P<proj>[A-Za-z]+)\\(?:include|src)\\(?:[A-Z][A-Za-z0-9]*\\){0,3}[A-Z][A-Za-z0-9]*\.h$")


def project_for(root: str) -> str | None:
    if root in ENGINE_ROOTS:
        return "Engine"
    if root == "Content":
        return "Content"
    return "Game" if root in GAME_MODULES else None


def read(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def read_header(path: Path) -> list[str]:
    with path.open(encoding="utf-8-sig", newline="") as f:
        return next(csv.reader(f), [])


def check_common(kind: str, r: dict, errs: list[str]) -> None:
    rid = r["id"]
    if r.get("confidence", "").strip() not in CONF:
        errs.append(f"{rid}: confidence must be high/med/low")
    if kind != "fields":
        name, loc = r["final_name"].strip(), r["final_location"].strip()
        root = name.split("::")[0]
        proj = project_for(root)
        if proj is None:
            errs.append(f"{rid}: namespace root '{root}' is not a §3 root")
        rx = SRC_LOC if kind == "globals" else HDR_LOC
        m = rx.match(loc)
        if not m:
            errs.append(f"{rid}: final_location '{loc}' must be Native\\<Project>\\"
                        + ("src\\...\\File.c" if kind == "globals" else "{include|src}\\...\\File.h"))
        elif proj and m["proj"] != proj:
            errs.append(f"{rid}: '{name}' belongs in Native\\{proj}, not Native\\{m['proj']}")
        elif proj == "Engine" and loc.split("\\")[3] != root:
            errs.append(f"{rid}: '{name}' belongs under Native\\Engine\\{loc.split(chr(92))[2]}\\{root}\\")
        if not r.get("purpose", "").strip():
            errs.append(f"{rid}: purpose is empty")
        elif len(r["purpose"]) > 200:
            errs.append(f"{rid}: purpose longer than 200 characters")


def check_batch(batch: str, partial: bool = False) -> tuple[list[str], dict[str, list[dict]]]:
    errs: list[str] = []
    outs: dict[str, list[dict]] = {}
    if batch not in {p.name for p in BATCHES.iterdir() if p.is_dir()}:
        return [f"{batch}: unknown batch"], outs
    for kind, header in HEADERS.items():
        src = BATCHES / batch / f"{kind}.csv"
        dst = OUTS / batch / f"{kind}.csv"
        if not src.exists():
            errs.append(f"{batch}/{kind}.csv: input missing")
            continue
        inp = read(src)
        if not inp:
            outs[kind] = []
            continue
        if not dst.exists():
            if partial:
                print(f"{batch}/{kind}.csv: 0/{len(inp)} rows saved")
                outs[kind] = []
                continue
            errs.append(f"{batch}/{kind}.csv: output missing")
            continue
        if read_header(dst) != header:
            errs.append(f"{batch}/{kind}.csv: header must be {','.join(header)}")
            continue
        rows = read(dst)
        for r in rows:
            if None in r or any(v is None for v in r.values()):
                errs.append(f"{batch}/{kind}.csv: row {r.get('id')} has a column count that does not match the header "
                            "(rows merged onto one line or a stray comma)")
        ids_in = {r["id"]: r for r in inp}
        seen = set()
        for r in rows:
            rid = r["id"]
            if rid not in ids_in:
                errs.append(f"{rid}: not in batch input")
                continue
            if rid in seen:
                errs.append(f"{rid}: duplicate row")
            seen.add(rid)
            name = r["final_name"].strip()
            if kind == "globals":
                if r["disposition"] not in DISPOSITIONS:
                    errs.append(f"{rid}: disposition must be one of {sorted(DISPOSITIONS)}")
                if not GLOBAL_RX.match(name):
                    errs.append(f"{rid}: '{name}' must be Namespace::lowerCamel (or Namespace::kPascal)")
                elif (r["disposition"] == "constexpr") != bool(re.search(r"::k[A-Z]", name)):
                    errs.append(f"{rid}: constexpr rows (and only they) use the kPascalCase form")
            elif kind == "types":
                if r["role"] not in ROLES:
                    errs.append(f"{rid}: role must be one of {sorted(ROLES)}")
                if not TYPE_RX.match(name):
                    errs.append(f"{rid}: '{name}' must be Namespace::PascalCase")
            else:
                is_enum = ids_in[rid].get("value", "") != ""
                if not (ENUMERATOR_RX if is_enum else FIELD_RX).match(name):
                    errs.append(f"{rid}: '{name}' must be " + ("PascalCase (enumerator)" if is_enum else "lowerCamelCase"))
                if not r.get("meaning", "").strip():
                    errs.append(f"{rid}: meaning is empty")
            if name and FORBIDDEN.search(name):
                errs.append(f"{rid}: '{name}' contains a forbidden token (unk/temp/address/vs_/D_/g_/...)")
            if not EVIDENCE.search(r.get("notes", "")):
                errs.append(f"{rid}: no evidence; notes need source:<file>:<line> of the code read "
                            "(or DataCrystal / same-as:T##### / view-of:G#####)")
            check_common(kind, r, errs)
        missing = set(ids_in) - seen
        if partial:
            print(f"{batch}/{kind}.csv: {len(seen & set(ids_in))}/{len(ids_in)} rows saved")
        elif missing:
            errs.append(f"{batch}/{kind}.csv: {len(missing)} ids missing, e.g. {sorted(missing)[:5]}")
        outs[kind] = rows
    return errs, outs


def cross_checks(all_outs: dict[str, dict[str, list[dict]]]) -> list[str]:
    errs: list[str] = []
    data = {r["id"]: r for r in read(LEDGERS / "DATA_LEDGER.csv")}
    fields = {r["id"]: r for r in read(LEDGERS / "FIELD_LEDGER.csv")}
    fn_c = {r["final_name"].replace("::", "_") for r in read(CATALOG) if r["final_name"]}

    # Already-applied names count too, so later partial passes stay consistent.
    g_rows = {r["id"]: r for r in data.values() if r["final_name"]}
    t_rows = {r["id"]: r for r in read(LEDGERS / "TYPE_LEDGER.csv") if r["final_name"]}
    f_rows = {r["id"]: r for r in fields.values() if r["final_name"]}
    for outs in all_outs.values():
        g_rows.update({r["id"]: r for r in outs.get("globals", [])})
        t_rows.update({r["id"]: r for r in outs.get("types", [])})
        f_rows.update({r["id"]: r for r in outs.get("fields", [])})

    def dup_of(r):
        m = re.search(r"dup-of:(G\d+)", r.get("notes", ""))
        return m.group(1) if m else None

    for gid, r in g_rows.items():
        target = dup_of(r)
        if target and target in g_rows and g_rows[target]["final_name"].strip() != r["final_name"].strip():
            errs.append(f"{gid}: '{r['final_name']}' differs from dup-of {target} '{g_rows[target]['final_name']}'")
        if target and target not in data:
            errs.append(f"{gid}: dup-of target {target} is not in DATA_LEDGER")
    by_c: dict[str, list[str]] = defaultdict(list)
    for gid, r in g_rows.items():
        if data.get(gid, {}).get("linkage") == "local-static" or dup_of(r):
            continue
        by_c[r["final_name"].replace("::", "_")].append(gid)
    for gid, r in t_rows.items():
        if "same-as:" in r.get("notes", ""):
            continue
        by_c[r["final_name"].replace("::", "_")].append(gid)
    all_types = {r["id"] for r in read(LEDGERS / "TYPE_LEDGER.csv")}
    for tid, r in t_rows.items():
        for target in re.findall(r"same-as:(T\d+)", r.get("notes", "")):
            if target not in all_types:
                errs.append(f"{tid}: same-as target {target} is not in TYPE_LEDGER")
            elif target in t_rows and t_rows[target]["final_name"].strip() != r["final_name"].strip():
                errs.append(f"{tid}: '{r['final_name']}' differs from same-as {target} "
                            f"'{t_rows[target]['final_name']}'")
    by_index = {(f["type_id"], f.get("index", "")): fid for fid, f in fields.items()}
    for tid, r in t_rows.items():
        for target in re.findall(r"same-as:(T\d+)", r.get("notes", "")):
            for fid, f in f_rows.items():
                if fields[fid]["type_id"] != tid:
                    continue
                twin = by_index.get((target, fields[fid].get("index", "")))
                if twin in f_rows and f_rows[twin]["final_name"] != f["final_name"]:
                    errs.append(f"{fid}: field '{f['final_name']}' differs from same-as {twin} "
                                f"'{f_rows[twin]['final_name']}'")
    for c, ids in by_c.items():
        if len(ids) > 1:
            errs.append(f"C name '{c}' used by {len(ids)} rows: {ids[:6]}")
        if c in fn_c:
            errs.append(f"C name '{c}' ({ids[0]}) collides with a function")
    per_type: dict[tuple[str, str], list[str]] = defaultdict(list)
    for fid, r in f_rows.items():
        per_type[(fields[fid]["type_id"], r["final_name"])].append(fid)
    for (tid, name), ids in per_type.items():
        if len(ids) > 1:
            errs.append(f"{tid}: field name '{name}' used {len(ids)} times: {ids}")
    return errs


def apply(all_outs: dict[str, dict[str, list[dict]]]) -> None:
    for kind, fname in LEDGER.items():
        path = LEDGERS / fname
        rows = read(path)
        cols = read_header(path)
        upd = {r["id"]: r for outs in all_outs.values() for r in outs.get(kind, [])}
        for row in rows:
            u = upd.get(row["id"])
            if u:
                for c in HEADERS[kind][1:]:
                    row[c] = u[c].strip()
                if kind != "fields":
                    row["named"] = "x"
        with path.open("w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
        print(f"{fname}: {len(upd)} rows applied")


def main() -> int:
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--check")
    g.add_argument("--check-all", action="store_true")
    g.add_argument("--apply", action="store_true")
    g.add_argument("--apply-batch", metavar="BATCH")
    g.add_argument("--check-partial", metavar="BATCH",
                   help="validate saved rows of an in-progress batch and report progress; never writes")
    a = ap.parse_args()

    selected = a.check or a.apply_batch or a.check_partial
    batches = [selected] if selected else sorted(p.name for p in BATCHES.iterdir() if p.is_dir())
    errs: list[str] = []
    all_outs = {}
    for b in batches:
        e, outs = check_batch(b, partial=bool(a.check_partial))
        errs += e
        all_outs[b] = outs
    errs += cross_checks(all_outs)
    for e in errs[:200]:
        print(e)
    if len(errs) > 200:
        print(f"... {len(errs) - 200} more")
    if errs:
        print(f"FAILED: {len(errs)} errors")
        return 1
    if a.apply or a.apply_batch:
        apply(all_outs)
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
