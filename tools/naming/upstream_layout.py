"""Compare struct/union layouts in the pinned reference (rood-reverseWin) with upstream rood-reverse main, and record
which types adopt the upstream layout.

Inputs:  build\\port\\index.json           pinned reference (tools\\port\\decomp_index.py)
         build\\port\\index_upstream.json  upstream main     (decomp_index.py --root resources\\rood-reverse --out ...)
Output:  Docs\\ledgers\\UPSTREAM_LAYOUTS.csv  adopted types; tools\\ledger\\build_symbol_ledgers.py reads it and
         emits those types' fields (and any upstream-only member types) from the upstream index.

Usage:
  python tools\\naming\\upstream_layout.py report [--batch B] [--ids T1,T2] [--out file]
  python tools\\naming\\upstream_layout.py adopt  (--batch B | --ids T1,T2 | --all)
  then python tools\\ledger\\build_symbol_ledgers.py to regenerate the ledgers.

Merge rule: each struct is split at the offsets where both trees start a member. Per group the finer side wins
(more leaf members, then more meaningful names; ties go upstream). Where both trees have the same members, the
upstream C types are used and the more meaningful name is kept. Field sources are recorded as pinned / upstream /
both / pinned+upstream-type; build_symbol_ledgers.py adds the hint `layout:upstream` to upstream-only fields, which
must be cited from resources\\rood-reverse (source:upstream:<file>:<line>).

Classes: same | type-only (same members and offsets, different C types) | layout (members split, merged, added
or moved) | fork-only (no upstream type of that name) | ambiguous (several different upstream layouts).
Adoption refuses a type whose already-named fields would change position, because field ids are positional.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGERS = ROOT / "Docs" / "ledgers"
PINNED = ROOT / "build" / "port" / "index.json"
UPSTREAM = ROOT / "build" / "port" / "index_upstream.json"
PINNED_ROOT = ROOT / "resources" / "rood-reverseWin"
UPSTREAM_ROOT = ROOT / "resources" / "rood-reverse"
ADOPTED = LEDGERS / "UPSTREAM_LAYOUTS.csv"
BATCHES = ROOT / "tools" / "naming" / "sym_batches"
ADOPT_COLS = ["type_id", "key", "name", "change", "upstream_file", "upstream_line", "pinned_fields", "merged_fields",
              "upstream_groups", "upstream_only_deps", "pinned_funcs_touching_changed_members"]
FIELD_HUMAN = ("final_name", "meaning", "confidence", "notes")


def layout_hash(t: dict) -> str:
    """Same hash as build_symbol_ledgers.layout(): identifies the ledger row of a multi-layout type."""
    sig = json.dumps([t.get("size"), [[x.get("name"), x.get("type"), x.get("bits"), x.get("value")]
                                      for x in t["fields"]]])
    return hashlib.sha1(sig.encode()).hexdigest()[:8]


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def by_key(index: dict) -> dict[str, list[dict]]:
    out: dict[str, list[dict]] = defaultdict(list)
    for t in index["types"].values():
        out[t["key"]].append(t)
    return out


def members(t: dict) -> list[tuple]:
    return [(x.get("offset"), x.get("name"), x.get("bits") or 0) for x in t["fields"]]


def pinned_layout(row: dict, pinned: dict[str, list[dict]]) -> dict | None:
    key, _, lh = row["key"].partition("#")
    cands = pinned.get(key, [])
    if lh:
        cands = [t for t in cands if layout_hash(t) == lh]
    return cands[0] if cands else None


def overlap(a: dict, b: dict) -> float:
    ma, mb = set(members(a)), set(members(b))
    return len(ma & mb) / max(1, len(ma | mb))


def upstream_layout(key: str, pin: dict, upstream: dict[str, list[dict]],
                    anon_by_file: dict[str, list[dict]]) -> tuple[dict | None, str]:
    """Best upstream match: same key (or, for anonymous types, same file), preferring the same file, then the
    largest member overlap. Anonymous types and same-named local types need the overlap to tell them apart."""
    anon = pin.get("anon") or "(unnamed" in key
    cands = anon_by_file.get(pin["file"], []) if anon else upstream.get(key, [])
    cands = [t for t in cands if t["fields"]]
    if not cands:
        return None, "fork-only"
    pool = [t for t in cands if t["file"] == pin["file"]] or cands
    scored = sorted(((overlap(pin, t), -abs(t["line"] - pin["line"]), i, t) for i, t in enumerate(pool)),
                    key=lambda s: (s[0], s[1]), reverse=True)
    best = scored[0]
    if anon and best[0] < 0.5:
        return None, "fork-only"
    if len(scored) > 1 and scored[1][0] == best[0] and best[0] < 0.5:
        return best[3], "ambiguous"
    return best[3], ""


def classify(pin: dict, up: dict | None, note: str) -> str:
    if note:
        return note
    if members(pin) != members(up):
        return "layout"
    if [x.get("type") for x in pin["fields"]] != [x.get("type") for x in up["fields"]] or pin.get("size") != up.get("size"):
        return "type-only"
    return "same"


def type_names(bykey: dict[str, list[dict]]) -> set[str]:
    return {k.split(":", 1)[1] for k in bykey}


def upstream_only_deps(t: dict, pinned: dict, upstream: dict, seen: set[str] | None = None) -> list[str]:
    """Upstream member types with no same-named type in the pinned reference (a same-named pinned type wins, since
    the copied code uses its members)."""
    seen = seen if seen is not None else set()
    names = type_names(pinned)
    out = []
    for dep in t.get("deps", {}):
        if dep in seen or dep.split(":", 1)[1] in names or dep not in upstream:
            continue
        seen.add(dep)
        out.append(dep)
        out += upstream_only_deps(upstream[dep][0], pinned, upstream, seen)
    return out


def struct_name(row: dict) -> str:
    return row["key"].split(":", 1)[1].split("#")[0]


GENERIC_FIELD = re.compile(r"^(unk|pad|_pad|filler|padding)[0-9A-Fa-fx_]*$|^field_?[0-9A-Fa-f]+$")


def type_text(root: Path, t: dict) -> str:
    p = root / t["file"]
    if not p.exists():
        return ""
    lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
    return "\n".join(x.strip() for x in lines[t["line"] - 1:t["end_line"]])


def resolve_struct(spelling: str, bykey: dict[str, list[dict]], seen: int = 0) -> dict | None:
    base = re.sub(r"\[.*\]|\bconst\b|\bvolatile\b|\*", "", spelling or "").strip()
    if "*" in (spelling or "") or seen > 4:
        return None
    base = base.removeprefix("struct ").removeprefix("union ").strip()
    for k in (f"struct:{base}", f"union:{base}"):
        if k in bykey and bykey[k][0]["fields"]:
            return bykey[k][0]
    td = bykey.get(f"typedef:{base}")
    if td:
        for dep in td[0].get("deps", {}):
            if dep.split(":")[0] in ("struct", "union") and dep in bykey and bykey[dep][0]["fields"]:
                return bykey[dep][0]
    return None


def leaves(x: dict, bykey: dict[str, list[dict]], depth: int = 0) -> int:
    t = resolve_struct(x.get("type", ""), bykey)
    if t is None or depth > 4 or "[" in (x.get("type") or ""):
        return 1
    return sum(leaves(y, bykey, depth + 1) for y in t["fields"]) or 1


def groups(pf: list[dict], uf: list[dict]) -> list[tuple[list[dict], list[dict]]]:
    """Split two member lists at the bit offsets where both start a member."""
    common = sorted({x["offset"] for x in pf} & {x["offset"] for x in uf})
    if not common or common[0] != 0:
        return [(pf, uf)]
    out = []
    for i, start in enumerate(common):
        end = common[i + 1] if i + 1 < len(common) else None
        def select_fields(fields, start_offset=start, end_offset=end):
            return [field for field in fields if field["offset"] >= start_offset
                    and (end_offset is None or field["offset"] < end_offset)]

        out.append((select_fields(pf), select_fields(uf)))
    return out


def merge(pin: dict, up: dict, pinned: dict[str, list[dict]], upstream: dict[str, list[dict]]) -> dict:
    """Merged member list: per group the finer side (leaf members, then meaningful names; ties go upstream). Where
    both sides have the same members, upstream types are used and the more meaningful name is kept."""
    fields, upstream_groups, size_from = [], 0, "pinned"
    is_union = pin["key"].startswith("union:")
    for pg, ug in groups(pin["fields"], up["fields"]) if not is_union else [(pin["fields"], up["fields"])]:
        if [(x["offset"], x.get("bits") or 0) for x in pg] == [(x["offset"], x.get("bits") or 0) for x in ug]:
            for p, u in zip(pg, ug, strict=True):
                keep_pinned = GENERIC_FIELD.match(u["name"] or "") and not GENERIC_FIELD.match(p["name"] or "")
                name = p["name"] if keep_pinned else u["name"]
                source = "both" if p["name"] == u["name"] else "pinned+upstream-type" if keep_pinned else "upstream"
                fields.append({**u, "name": name, "source": source})
            size_from = "upstream" if ug else size_from
            continue
        def score(members, index):
            return (sum(leaves(member, index) for member in members),
                    sum(1 for member in members if not GENERIC_FIELD.match(member.get("name") or "")))

        take_up = score(ug, upstream) >= score(pg, pinned)
        chosen = ug if take_up else pg
        upstream_groups += take_up
        other = {(x["offset"], x["name"]) for x in (pg if take_up else ug)}
        fields += [{**x, "source": "both" if (x["offset"], x["name"]) in other else "upstream" if take_up else "pinned"}
                   for x in chosen]
        size_from = "upstream" if take_up else "pinned"
    names = [f["name"] for f in fields if f["name"]]
    dup = sorted({n for n in names if names.count(n) > 1})
    size = up.get("size") if size_from == "upstream" else pin.get("size")
    return {"fields": fields, "size": size, "upstream_groups": upstream_groups, "duplicates": dup}


def funcs_touching(name: str, changed: set[str], index: dict) -> list[str]:
    out = []
    for fn in index["functions"].values():
        refs = fn.get("refs") or {}
        fields = refs.get("field", {}) if isinstance(refs, dict) else {}
        if any(f"{name}.{m}" in fields for m in changed):
            out.append(fn["name"])
    return sorted(set(out))


def select_rows(types: list[dict], batch: str | None, ids: str | None) -> list[dict]:
    if ids:
        want = set(ids.split(","))
        return [t for t in types if t["id"] in want]
    if batch:
        want = {r["id"] for r in read_csv(BATCHES / batch / "types.csv")}
        return [t for t in types if t["id"] in want]
    return [t for t in types if t["kind"] in ("struct", "union", "typedef")]


def analyse(rows: list[dict], pinned_ix: dict, upstream_ix: dict) -> list[dict]:
    pinned, upstream = by_key(pinned_ix), by_key(upstream_ix)
    anon_by_file: dict[str, list[dict]] = defaultdict(list)
    for t in upstream_ix["types"].values():
        if t.get("anon") or "(unnamed" in t["key"]:
            anon_by_file[t["file"]].append(t)
    out = []
    for row in rows:
        pin = pinned_layout(row, pinned)
        if pin is None or not pin["fields"]:
            continue
        key = row["key"].partition("#")[0]
        up, note = upstream_layout(key, pin, upstream, anon_by_file)
        cls = classify(pin, up, note)
        if cls in ("layout", "type-only") and type_text(PINNED_ROOT, pin) == type_text(UPSTREAM_ROOT, up):
            cls = "same"  # identical source; the layouts differ only through per-overlay macros
        rec = {"row": row, "pin": pin, "up": up, "class": cls, "removed": [], "added": [], "retyped": [],
               "deps": [], "funcs": [], "merge": None}
        if up is not None and cls in ("layout", "type-only"):
            pm = {(x.get("offset"), x.get("name")): x for x in pin["fields"]}
            um = {(x.get("offset"), x.get("name")): x for x in up["fields"]}
            rec["removed"] = [x for k, x in pm.items() if k not in um]
            rec["added"] = [x for k, x in um.items() if k not in pm]
            rec["retyped"] = [(pm[k], um[k]) for k in pm if k in um and pm[k].get("type") != um[k].get("type")]
            rec["merge"] = m = merge(pin, up, pinned, upstream)
            kept = {(x["offset"], x["name"]) for x in m["fields"]}
            replaced = {x["name"] for x in pin["fields"] if (x["offset"], x["name"]) not in kept}
            changed = replaced | {a["name"] for a, b in rec["retyped"] if (a["offset"], a["name"]) in kept}
            rec["funcs"] = funcs_touching(struct_name(row), changed, pinned_ix)
            seen: set[str] = set()
            pinned_names = type_names(pinned)
            for x in m["fields"]:
                if x["source"] != "upstream":
                    continue
                base = re.sub(r"\[.*\]|\*|\bconst\b", "", x.get("type") or "").strip()
                base = base.removeprefix("struct ").removeprefix("union ").strip()
                if base in pinned_names:
                    continue
                for k in (f"typedef:{base}", f"struct:{base}", f"union:{base}"):
                    if k in upstream and k not in seen:
                        seen.add(k)
                        rec["deps"] += [k, *upstream_only_deps(upstream[k][0], pinned, upstream, seen)]
        out.append(rec)
    return out


def fmt_member(x: dict) -> str:
    off = x.get("offset")
    pos = f"+0x{off // 8:X}" + (f".{off % 8}" if off is not None and off % 8 else "") if off is not None and off >= 0 else "+?"
    bits = f":{x['bits']}" if x.get("bits") else ""
    return f"{pos} {x.get('type')} {x.get('name')}{bits}"


def report(recs: list[dict], out: Path | None) -> None:
    lines, counts = [], defaultdict(int)
    for r in recs:
        counts[r["class"]] += 1
        if r["class"] in ("same",):
            continue
        row = r["row"]
        lines.append(f"## {row['id']} {row['name']} [{r['class']}]  pinned {r['pin']['file']}:{r['pin']['line']}"
                     + (f"  upstream {r['up']['file']}:{r['up']['line']}" if r["up"] else ""))
        for x in r["removed"]:
            lines.append(f"  - {fmt_member(x)}")
        for x in r["added"]:
            lines.append(f"  + {fmt_member(x)}")
        for a, b in r["retyped"]:
            lines.append(f"  ~ {fmt_member(a)}  ->  {b.get('type')}")
        if r["merge"]:
            m = r["merge"]
            lines.append(f"  merged ({m['upstream_groups']} group(s) from upstream, size {m['size']}):")
            lines += [f"    {x['source']:22} {fmt_member(x)}" for x in m["fields"]]
            if m["duplicates"]:
                lines.append(f"  DUPLICATE merged names: {', '.join(m['duplicates'])}")
        if r["deps"]:
            lines.append(f"  upstream-only member types: {', '.join(r['deps'])}")
        if r["funcs"]:
            lines.append(f"  pinned functions using changed members: {', '.join(r['funcs'])}")
    summary = "  ".join(f"{k}={v}" for k, v in sorted(counts.items()))
    text = "\n".join([f"# upstream layout report: {summary}", *lines]) + "\n"
    if out:
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="utf-8")
        print(f"{out}: {summary}")
    else:
        sys.stdout.write(text)


def adopt(recs: list[dict]) -> int:
    fields = read_csv(LEDGERS / "FIELD_LEDGER.csv")
    human = defaultdict(list)
    for f in fields:
        if any(f.get(c) for c in FIELD_HUMAN):
            human[f["type_id"]].append(f)
    existing = {r["type_id"]: r for r in read_csv(ADOPTED)} if ADOPTED.exists() else {}
    refused = 0
    for r in recs:
        if r["class"] not in ("layout", "type-only"):
            continue
        row, up, m = r["row"], r["up"], r["merge"]
        why = ""
        if m["duplicates"]:
            why = f"merged layout repeats {', '.join(m['duplicates'])}"
        for f in human.get(row["id"], []):
            n = int(f["index"])
            p = r["pin"]["fields"][n] if n < len(r["pin"]["fields"]) else None
            q = m["fields"][n] if n < len(m["fields"]) else None
            if not (p and q and p["offset"] == q["offset"] and p["name"] == q["name"]):
                why = why or f"filled field {f['id']} {f['name']} would move"
        if why:
            print(f"refused {row['id']} {row['name']}: {why}")
            refused += 1
            continue
        existing[row["id"]] = {"type_id": row["id"], "key": row["key"], "name": row["name"], "change": r["class"],
                               "upstream_file": up["file"], "upstream_line": up["line"],
                               "pinned_fields": len(r["pin"]["fields"]), "merged_fields": len(m["fields"]),
                               "upstream_groups": m["upstream_groups"], "upstream_only_deps": " ".join(r["deps"]),
                               "pinned_funcs_touching_changed_members": " ".join(r["funcs"])}
    with ADOPTED.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=ADOPT_COLS, lineterminator="\n")
        w.writeheader()
        for k in sorted(existing):
            w.writerow(existing[k])
    print(f"{ADOPTED}: {len(existing)} adopted types, {refused} refused")
    return 1 if refused else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["report", "adopt"])
    ap.add_argument("--batch")
    ap.add_argument("--ids")
    ap.add_argument("--all", action="store_true", help="adopt every type whose layout differs")
    ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    if a.cmd == "adopt" and not (a.batch or a.ids or a.all):
        ap.error("adopt needs --batch, --ids or --all")
    pinned_ix = json.loads(PINNED.read_text(encoding="utf-8"))
    upstream_ix = json.loads(UPSTREAM.read_text(encoding="utf-8"))
    if not any("offset" in x for t in pinned_ix["types"].values() for x in t["fields"]):
        sys.exit("index.json has no field offsets; re-run tools\\port\\decomp_index.py")
    recs = analyse(select_rows(read_csv(LEDGERS / "TYPE_LEDGER.csv"), a.batch, a.ids), pinned_ix, upstream_ix)
    if a.cmd == "report":
        report(recs, a.out)
        return 0
    return adopt(recs)


if __name__ == "__main__":
    raise SystemExit(main())
