"""Generate the global-data and type ledgers from the decomp index (Phase 1a naming pass).

Inputs  (run tools\\port\\decomp_index.py and tools\\port\\data_inventory.py first):
  build\\port\\index.json   functions / types / refs
  build\\port\\data.json    storage identities (one per real piece of storage)
  Docs\\ledgers\\FUNCTION_CATALOG.csv   final function names / locations

Outputs (stable ids; human-filled columns are preserved across re-runs):
  Docs\\ledgers\\DATA_LEDGER.csv    G##### one row per storage identity
  Docs\\ledgers\\TYPE_LEDGER.csv    T##### one row per distinct (type, layout)
  Docs\\ledgers\\FIELD_LEDGER.csv   T#####.NNN one row per struct/union field or enumerator
"""
from __future__ import annotations

import ast
import csv
import hashlib
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGERS = ROOT / "Docs" / "ledgers"
INDEX = ROOT / "build" / "port" / "index.json"
INDEX_UPSTREAM = ROOT / "build" / "port" / "index_upstream.json"
DATA = ROOT / "build" / "port" / "data.json"
CATALOG = LEDGERS / "FUNCTION_CATALOG.csv"
ADOPTED = LEDGERS / "UPSTREAM_LAYOUTS.csv"

GENERIC_GLOBAL = re.compile(r"^(D|B|jtbl|jpt|g)_[0-9A-Fa-f]{6,8}(_\w+)?$")
GENERIC_TYPE = re.compile(r"(^|_)(D|func)_[0-9A-Fa-f]{8}|_t\d*$|^unk|^Unk")
GENERIC_FIELD = re.compile(r"^(unk|pad|_pad|filler)[0-9A-Fa-fx_]*$|^field_?[0-9A-Fa-f]+$")
RESIDENT = ("SLUS_010.40", "BATTLE/BATTLE.PRG")

PROGRESS = ["copied", "compiles", "named", "understood", "modernized", "cpp", "verified"]
DATA_COLS = ["id", "key", "name", "generic", "overlay", "linkage", "segment", "address", "file_offset", "size",
             "type", "type_id", "has_pointers", "init", "instances", "files", "used_by", "proposed_system",
             "proposed_disposition", "disposition", "final_name", "final_location", "purpose", "confidence",
             *PROGRESS, "notes"]
TYPE_COLS = ["id", "key", "kind", "name", "generic", "aliases", "size", "fields", "generic_fields", "layouts",
             "decls", "files", "used_by_funcs", "used_by_globals", "proposed_system", "role", "final_name",
             "final_location", "purpose", "confidence", *PROGRESS, "notes"]
FIELD_COLS = ["id", "type_id", "type_name", "index", "name", "generic", "type", "bits", "value", "hints",
              "final_name", "meaning", "confidence", "notes"]
DATA_HUMAN = {"disposition", "final_name", "final_location", "purpose", "confidence", *PROGRESS, "notes"}
TYPE_HUMAN = {"role", "final_name", "final_location", "purpose", "confidence", *PROGRESS, "notes"}
FIELD_HUMAN = {"final_name", "meaning", "confidence", "notes"}


def overlay_of(src_file: str) -> str:
    parts = src_file.replace("\\", "/").split("/")
    if parts[0] == "src":
        parts = parts[1:]
    if parts and parts[0] == "SLUS_010.40":
        return "SLUS_010.40"
    return "/".join(parts[:2]) if len(parts) > 2 else ""


def system_of(location: str) -> str:
    # Native\Game\src\Battle\Ai\Navigation.c -> Game.Battle
    p = location.replace("/", "\\").split("\\")
    if len(p) >= 4 and p[0] == "Native":
        sub = p[3] if not p[3].endswith((".c", ".cpp", ".h")) else ""
        return f"{p[1]}.{sub}" if sub else p[1]
    return ""


OVERLAY_SYSTEM = {"EFFECT": "Game.Effects", "MENU": "Game.Menu", "TITLE": "Game.Title", "BATTLE": "Game.Battle",
                  "ENDING": "Game.Ending", "GIM": "Game.Field", "SMALL": "Game.Field"}


def fallback_system(path_or_overlay: str) -> str:
    p = path_or_overlay.replace("\\", "/").removeprefix("src/")
    return OVERLAY_SYSTEM.get(p.split("/")[0], "")


def load_existing(path: Path, key: str) -> dict[str, dict]:
    if not path.exists():
        return {}
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows or key not in rows[0] or "id" not in rows[0]:
        return {}
    return {r[key]: r for r in rows}


def assign_ids(keys: list[str], old: dict[str, dict], prefix: str) -> dict[str, str]:
    used = {r["id"] for r in old.values()}
    nxt = max((int(i[1:]) for i in used if i[1:].isdigit()), default=-1) + 1
    ids = {}
    for k in keys:
        if k in old:
            ids[k] = old[k]["id"]
        else:
            ids[k] = f"{prefix}{nxt:05d}"
            nxt += 1
    return ids


def write(path: Path, cols: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
        w.writeheader()
        for r in rows:
            w.writerow({c: r.get(c, "") for c in cols})


def parse_refs(v):
    return ast.literal_eval(v) if isinstance(v, str) else (v or {})


def field_hints() -> dict[str, list[str]]:
    """fields:Type.unkN=meaning notes left by the function naming agents."""
    hints: dict[str, list[str]] = defaultdict(list)
    with CATALOG.open(encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f):
            for part in r.get("notes", "").split(";"):
                part = part.strip()
                if part.startswith("fields:") and "=" in part:
                    lhs, rhs = part[7:].split("=", 1)
                    if "." in lhs:
                        hints[lhs.strip()].append(rhs.strip())
    return hints


def adopted_layouts(index: dict) -> tuple[dict[str, dict], list[dict]]:
    """Types listed in UPSTREAM_LAYOUTS.csv: row key -> merged layout (tools/naming/upstream_layout.py), plus the
    upstream-only member types those layouts need."""
    if not ADOPTED.exists():
        return {}, []
    with ADOPTED.open(encoding="utf-8", newline="") as f:
        rows = [r for r in csv.DictReader(f)]
    if not rows:
        return {}, []
    if not INDEX_UPSTREAM.exists():
        sys.exit(f"{ADOPTED.name} lists adopted layouts; run tools/port/decomp_index.py --root resources/rood-reverse "
                 f"--out {INDEX_UPSTREAM.relative_to(ROOT)} first")
    sys.path.insert(0, str(ROOT / "tools" / "naming"))
    import upstream_layout  # noqa: E402

    upstream = json.loads(INDEX_UPSTREAM.read_text(encoding="utf-8"))
    recs = upstream_layout.analyse([{"id": r["type_id"], "key": r["key"], "name": r["name"]} for r in rows],
                                   index, upstream)
    out, extra, seen = {}, [], set()
    for rec in recs:
        if rec["merge"] is None:
            sys.exit(f"{rec['row']['id']} {rec['row']['name']}: adopted layout no longer differs ({rec['class']}); "
                     f"remove it from {ADOPTED.name}")
        out[rec["row"]["key"]] = rec["merge"]
        for dep in rec["deps"]:
            if dep not in seen:
                seen.add(dep)
                extra += [{**t, "file": "upstream:" + t["file"]} for t in upstream_layout.by_key(upstream)[dep][:1]]
    return out, extra


def main() -> int:
    index = json.loads(INDEX.read_text(encoding="utf-8"))
    idents = json.loads(DATA.read_text(encoding="utf-8"))
    sdk_types = set(index.get("sdk_types", []))
    merged, upstream_types = adopted_layouts(index)

    catalog: dict[str, dict] = {}
    with CATALOG.open(encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f):
            catalog[f"{r['name']}@src/{r['file']}:{r['line']}"] = r

    # ---- types: group declarations by (key, layout) -------------------------------------------
    def layout(t: dict) -> str:
        sig = json.dumps([t.get("size"), [[x.get("name"), x.get("type"), x.get("bits"), x.get("value")]
                                          for x in t["fields"]]])
        return hashlib.sha1(sig.encode()).hexdigest()[:8]

    by_key: dict[str, dict[str, list[dict]]] = defaultdict(lambda: defaultdict(list))
    for t in [*index["types"].values(), *upstream_types]:
        name = t["key"].split(":", 1)[1]
        if name in sdk_types:
            continue
        by_key[t["key"]][layout(t)].append(t)

    # typedef X -> struct Y collapses into the struct row as an alias
    alias_of: dict[str, str] = {}
    for key, lays in by_key.items():
        if key.startswith("typedef:"):
            t = next(iter(lays.values()))[0]
            deps = [k for k in t.get("deps", {}) if k.split(":")[0] in ("struct", "union", "enum")]
            if not t["fields"] and len(deps) == 1 and deps[0] in by_key:
                alias_of[key] = deps[0]

    func_type_refs: Counter = Counter()
    type_systems: dict[str, Counter] = defaultdict(Counter)
    func_var_refs: list[tuple[str, str, str, str, str]] = []  # (var, fn overlay, system, fn file, fn name)
    type_funcs: dict[str, list[str]] = defaultdict(list)
    func_label: dict[str, str] = {}
    for fk, fn in index["functions"].items():
        refs = parse_refs(fn.get("refs"))
        cat = catalog.get(fk, {})
        sysname = system_of(cat.get("final_location", "")) if cat else ""
        func_label[f'{fn["file"]}:{fn["name"]}'] = (
            f'{cat.get("final_name") or fn["name"]} ({fn["file"].removeprefix("src/")}:{fn["line"]} {fn["name"]})')
        for tk in refs.get("type", {}):
            tk = alias_of.get(tk, tk)
            func_type_refs[tk] += 1
            type_funcs[tk].append(f'{fn["file"]}:{fn["name"]}')
            if sysname:
                type_systems[tk][sysname] += 1
        ov = overlay_of(fn["file"])
        for vn in refs.get("var", {}):
            func_var_refs.append((vn, ov, sysname, fn["file"], fn["name"]))

    # ---- globals -------------------------------------------------------------------------------
    by_name: dict[str, list[dict]] = defaultdict(list)
    for g in idents:
        by_name[g["name"]].append(g)

    def resolve(name: str, ov: str, file: str, func: str) -> dict | None:
        cands = by_name.get(name)
        if not cands:
            return None
        if len(cands) == 1:
            return cands[0]
        for c in cands:
            if c["linkage"] == "local-static" and file in c["files"] and func in c.get("scopes", []):
                return c
        for c in cands:
            if c["linkage"] == "file-static" and file in c["files"]:
                return c
        for c in cands:
            if c["linkage"] in ("local-static", "file-static"):
                continue
            if c.get("overlay") == ov or ov in c.get("instances", []):
                return c
        for c in cands:
            if c.get("overlay") in RESIDENT and c["linkage"] not in ("local-static", "file-static"):
                return c
        return None

    used_by: Counter = Counter()
    g_funcs: dict[str, list[str]] = defaultdict(list)
    g_systems: dict[str, Counter] = defaultdict(Counter)
    for vn, ov, sysname, file, func in func_var_refs:
        g = resolve(vn, ov, file, func)
        if g:
            used_by[g["key"]] += 1
            g_funcs[g["key"]].append(f"{file}:{func}")
            if sysname:
                g_systems[g["key"]][sysname] += 1

    type_globals: Counter = Counter()
    for g in idents:
        if g.get("type_key"):
            tk = alias_of.get(g["type_key"], g["type_key"])
            type_globals[tk] += 1
            sysc = g_systems.get(g["key"])
            if sysc and tk not in type_systems:
                type_systems.setdefault("~" + tk, Counter()).update(sysc)

    # ---- TYPE_LEDGER ---------------------------------------------------------------------------
    t_rows_src = []
    for key, lays in sorted(by_key.items()):
        if key in alias_of:
            continue
        multi = len(lays) > 1
        for lh, decls in sorted(lays.items(), key=lambda kv: -len(kv[1])):
            row_key = f"{key}#{lh}" if multi else key
            t_rows_src.append((row_key, key, lh, decls, len(lays)))
    old_t = load_existing(LEDGERS / "TYPE_LEDGER.csv", "key")
    tids = assign_ids([r[0] for r in t_rows_src], old_t, "T")
    tid_of_key: dict[str, str] = {}
    aliases: dict[str, list[str]] = defaultdict(list)
    for a, target in alias_of.items():
        aliases[target].append(a.split(":", 1)[1])

    hints = field_hints()
    old_f = load_existing(LEDGERS / "FIELD_LEDGER.csv", "id")
    t_rows, f_rows = [], []
    for row_key, key, lh, decls, nlay in t_rows_src:
        t = decls[0]
        m = merged.get(row_key)
        t_fields = m["fields"] if m else t["fields"]
        kind, name = key.split(":", 1)
        tid = tids[row_key]
        tid_of_key.setdefault(key, tid)
        names = [name, *aliases.get(key, [])]
        gen_fields = sum(1 for x in t_fields if GENERIC_FIELD.match(x.get("name") or ""))
        sysc = type_systems.get(key) or type_systems.get("~" + key) or Counter(
            {s: 1 for s in [fallback_system(t["file"])] if s})
        prev = old_t.get(row_key, {})
        row = {
            "id": tid, "key": row_key, "kind": kind, "name": name,
            "generic": int(bool(GENERIC_TYPE.search(name))), "aliases": " ".join(aliases.get(key, [])),
            "size": m["size"] if m else t.get("size", ""), "fields": len(t_fields), "generic_fields": gen_fields,
            "layouts": nlay, "decls": len(decls),
            "files": " ".join(sorted({d["file"].removeprefix("src/") for d in decls})[:6]),
            "used_by_funcs": func_type_refs.get(key, 0), "used_by_globals": type_globals.get(key, 0),
            "proposed_system": sysc.most_common(1)[0][0] if sysc else "",
        }
        row.update({c: prev.get(c, "") for c in TYPE_HUMAN})
        t_rows.append(row)
        for n, x in enumerate(t_fields):
            fid = f"{tid}.{n:03d}"
            fname = x.get("name") or ""
            h = sorted({v for nm in names for v in hints.get(f"{nm}.{fname}", [])})
            if x.get("source") == "upstream":
                h.append("layout:upstream")
            fr = {"id": fid, "type_id": tid, "type_name": name, "index": n, "name": fname,
                  "generic": int(bool(GENERIC_FIELD.match(fname))), "type": x.get("type", ""),
                  "bits": x.get("bits", "") or "", "value": x.get("value", ""), "hints": " | ".join(h)}
            fr.update({c: old_f.get(fid, {}).get(c, "") for c in FIELD_HUMAN})
            f_rows.append(fr)

    # ---- DATA_LEDGER ---------------------------------------------------------------------------
    def addr_sort(g):
        a = g.get("address")
        return (g.get("overlay") or "", int(a, 16) if a else 1 << 40, g["key"])

    idents.sort(key=addr_sort)
    old_d = load_existing(LEDGERS / "DATA_LEDGER.csv", "key")
    gids = assign_ids([g["key"] for g in idents], old_d, "G")

    def propose(g) -> str:
        seg, link = g.get("segment"), g.get("linkage")
        if seg == "asset":
            return "asset"
        if g["name"].startswith("vs_scratch"):
            return "state"
        if seg in ("bss",) or (link == "local-static" and not g.get("init")):
            return "state"
        if seg == "rodata" or str(g.get("type", "")).startswith("const "):
            return "constexpr"
        if seg in ("data", "unit-data", "c-defined"):
            size = g.get("size") or 0
            return "catalog" if isinstance(size, int) and (size >= 32 or size < 0) and "[" in str(g.get("type")) else "state"
        return "?"

    d_rows = []
    for g in idents:
        k = g["key"]
        tk = alias_of.get(g.get("type_key", ""), g.get("type_key", ""))
        sysc = g_systems.get(k) or Counter({s: 1 for s in [fallback_system(g.get("overlay") or k.split("@")[1])] if s})
        prev = old_d.get(k, {})
        row = {
            "id": gids[k], "key": k, "name": g["name"], "generic": int(bool(GENERIC_GLOBAL.match(g["name"]))),
            "overlay": g.get("overlay") or "", "linkage": g.get("linkage", ""), "segment": g.get("segment", ""),
            "address": g.get("address", ""), "file_offset": g.get("file_offset", ""), "size": g.get("size", ""),
            "type": g.get("type", ""), "type_id": tid_of_key.get(tk, ""),
            "has_pointers": int(bool(g.get("has_pointers"))), "init": int(bool(g.get("init"))),
            "instances": " ".join(g.get("instances", [])),
            "files": " ".join(f.removeprefix("src/") for f in g.get("files", [])),
            "used_by": used_by.get(k, 0), "proposed_system": sysc.most_common(1)[0][0] if sysc else "",
            "proposed_disposition": propose(g),
        }
        row.update({c: prev.get(c, "") for c in DATA_HUMAN})
        d_rows.append(row)

    # Sidecar for the naming batches: who uses each global/type (final function names + decomp origin).
    ctx = {"globals": {gids[k]: [func_label.get(f, f) for f in sorted(set(v))] for k, v in g_funcs.items()},
           "types": {tid_of_key[k]: [func_label.get(f, f) for f in sorted(set(v))]
                     for k, v in type_funcs.items() if k in tid_of_key}}
    (ROOT / "build" / "port" / "symbol_context.json").write_text(json.dumps(ctx, indent=0), encoding="utf-8")

    write(LEDGERS / "DATA_LEDGER.csv", DATA_COLS, d_rows)
    write(LEDGERS / "TYPE_LEDGER.csv", TYPE_COLS, t_rows)
    write(LEDGERS / "FIELD_LEDGER.csv", FIELD_COLS, f_rows)

    print(f"DATA_LEDGER  {len(d_rows)} identities, generic {sum(r['generic'] for r in d_rows)}, "
          f"unreferenced {sum(1 for r in d_rows if not r['used_by'])}, "
          f"no system {sum(1 for r in d_rows if not r['proposed_system'])}")
    print("  dispositions", Counter(r["proposed_disposition"] for r in d_rows))
    print(f"TYPE_LEDGER  {len(t_rows)} types ({len(alias_of)} typedef aliases folded), "
          f"generic names {sum(r['generic'] for r in t_rows)}, multi-layout {sum(1 for r in t_rows if r['layouts'] > 1)}")
    print(f"FIELD_LEDGER {len(f_rows)} fields, generic {sum(r['generic'] for r in f_rows)}, "
          f"with hints {sum(1 for r in f_rows if r['hints'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
