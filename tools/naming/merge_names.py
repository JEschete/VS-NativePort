"""Validate naming-pass outputs and apply them to FUNCTION_CATALOG.csv (Docs\\NAMING_CONVENTIONS.md).

  python tools\\naming\\merge_names.py --check BATTLE_03   validate one batch output (agents run this)
  python tools\\naming\\merge_names.py --check-all         validate every output + cross-batch uniqueness
  python tools\\naming\\merge_names.py --apply             validate all, then write final_name/final_location/purpose
                                                           (+ mechanical names for effect template instances)
Exit code 1 on any error.
"""
from __future__ import annotations

import argparse
import collections
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "Docs" / "ledgers" / "FUNCTION_CATALOG.csv"
BATCHES = ROOT / "tools" / "naming" / "batches"
OUTS = ROOT / "tools" / "naming" / "out"
HEADER = ["id", "final_name", "final_location", "purpose", "confidence", "notes"]

# Hybrid layout (gen_solution.py): engine systems in Native\Engine\src\<Root>\, content in Native\Content\src\,
# game modules in Native\Game\src\<Root>\.
ENGINE_ROOTS = ["Core", "Platform", "Render", "Audio", "Input", "Movie", "Save"]
GAME_MODULES = ["Boot", "Title", "Field", "Battle", "Actors", "Items", "Menu", "Event", "Effects", "Ending", "State"]
FORBIDDEN = re.compile(r"(^|::)(Func|Sub|Unk|Unknown|Helper|Do|Process)($|::|[0-9A-F]{6,})|[0-9A-Fa-f]{8}|^Vs|::Vs[A-Z_]",
                       re.I)
NAME_RX = re.compile(r"^([A-Z][A-Za-z0-9]*)(::[A-Z][A-Za-z0-9]*){1,3}$")
LOC_RX = re.compile(r"^Native\\(?P<proj>[A-Za-z]+)\\src\\(?P<rest>(?:[A-Z][A-Za-z0-9]*\\){0,3}[A-Z][A-Za-z0-9]*\.c)$")
CONF = {"high", "med", "low"}


def expected_dir(root: str) -> str | None:
    if root in ENGINE_ROOTS:
        return f"Native\\Engine\\src\\{root}\\"
    if root == "Content":
        return "Native\\Content\\src\\"
    if root in GAME_MODULES:
        return f"Native\\Game\\src\\{root}\\"
    return None


def c_spelling(name: str) -> str:
    return name.replace("::", "_")


def read_csv(path: Path) -> list[dict]:
    with path.open(newline="", encoding="utf-8-sig") as fh:
        return list(csv.DictReader(fh))


def check_row(r: dict) -> list[str]:
    errs = []
    name, loc = r.get("final_name", "").strip(), r.get("final_location", "").strip()
    if not NAME_RX.match(name):
        errs.append(f"final_name '{name}' must be Namespace::Name PascalCase (1-3 namespace levels)")
    elif FORBIDDEN.search(name):
        errs.append(f"final_name '{name}' contains a forbidden/placeholder token")
    root = name.split("::")[0] if name else ""
    exp = expected_dir(root)
    if name and exp is None:
        errs.append(f"namespace root '{root}' is not a known system/module")
    m = LOC_RX.match(loc)
    if not m:
        errs.append(f"final_location '{loc}' must be Native\\<Proj>\\src\\[<Dir>\\[<Dir>\\]]<File>.c")
    elif exp and not loc.startswith(exp):
        errs.append(f"final_location '{loc}' does not match namespace root '{root}' (expected under {exp})")
    elif exp and name:
        path = loc[len(exp):-2].split("\\")  # folders below the system/module root + file stem
        sub = name.split("::")[1:-1]
        ok = path[:-1] == sub or (sub and path[-len(sub):] == sub and len(path) - len(sub) <= 1)
        # asm copy of an effect template instance: named and placed like the template rows (template_rows)
        ok = ok or (path[:-1] == ["Templates"] and re.fullmatch(r"Effects::Plg\d+::" + re.escape(path[-1]), name)
                    and "dup-of:" in (r.get("notes") or ""))
        if not ok:
            errs.append(f"namespace '{'::'.join(name.split('::')[:-1])}' must match the folders below {exp} "
                        f"or end in the file name (got '{loc[len(exp):]}')")
    if "Triage" in loc:
        errs.append("final_location must not be Triage")
    if len(name.split("::")[-1]) > 40:
        errs.append("Name part longer than 40 chars")
    p = r.get("purpose", "").strip()
    if not p or len(p) > 200 or p.lower().startswith(("unknown", "todo", "tbd")):
        errs.append(f"purpose missing/too long/placeholder ({len(p)} chars)")
    if r.get("confidence", "").strip() not in CONF:
        errs.append(f"confidence '{r.get('confidence')}' must be high/med/low")
    return errs


def check_batch(batch: str) -> tuple[list[str], list[dict]]:
    errs: list[str] = []
    src, out = BATCHES / f"{batch}.csv", OUTS / f"{batch}.csv"
    if not src.exists():
        return [f"{batch}: no such batch"], []
    if not out.exists():
        return [f"{batch}: output {out} missing"], []
    with out.open(newline="", encoding="utf-8-sig") as fh:
        header = next(csv.reader(fh), [])
    if [h.strip() for h in header] != HEADER:
        errs.append(f"{batch}: header must be {','.join(HEADER)} (got {','.join(header)})")
    want = [r["id"] for r in read_csv(src)]
    rows = read_csv(out)
    got = [r.get("id", "") for r in rows]
    missing, extra = set(want) - set(got), set(got) - set(want)
    dups = [i for i, n in collections.Counter(got).items() if n > 1]
    if missing:
        errs.append(f"{batch}: {len(missing)} ids missing, e.g. {sorted(missing)[:5]}")
    if extra:
        errs.append(f"{batch}: {len(extra)} unexpected ids, e.g. {sorted(extra)[:5]}")
    if dups:
        errs.append(f"{batch}: duplicate ids {dups[:5]}")
    for r in rows:
        for e in check_row(r):
            errs.append(f"{batch}:{r.get('id')}: {e}")
    return errs, rows


def dup_target(notes: str) -> str | None:
    m = re.search(r"dup-of:(F\d{5})", notes or "")
    return m.group(1) if m else None


def check_unique(all_rows: dict[str, dict], extra_targets: dict[str, dict] | None = None) -> list[str]:
    """`extra_targets`: catalog rows named by rule (template instances) that dup-of may point at."""
    errs = []
    by_c: dict[str, list[str]] = collections.defaultdict(list)
    for rid, r in all_rows.items():
        if dup_target(r.get("notes", "")):
            continue
        by_c[c_spelling(r["final_name"].strip())].append(rid)
    for cname, ids in by_c.items():
        if len(ids) > 1:
            errs.append(f"C name '{cname}' used by {ids} (rename, or mark identical code with dup-of:<id>)")
    for rid, r in all_rows.items():
        t = dup_target(r.get("notes", ""))
        if t:
            tr = all_rows.get(t) or (extra_targets or {}).get(t)
            if not tr:
                errs.append(f"{rid}: dup-of target {t} not in outputs (yet)")
            elif tr["final_name"].strip() != r["final_name"].strip():
                errs.append(f"{rid}: dup-of {t} but final_name differs ({r['final_name']} vs {tr['final_name']})")
    return errs


DECOMP_SRC = ROOT / "resources" / "rood-reverseWin" / "src"


def template_header(r: dict, bodies: dict | None = None) -> str | None:
    """The template header an instance binding feeds: the first `#include "src/EFFECT/<stem>.h"` after the
    binding's `#define` that defines the bound macro's function (plugins bind the same macro for several headers,
    e.g. trails9 and trails5 in PLG057, and bind render functions before the setup header that only calls them)."""
    try:
        lines = (DECOMP_SRC / r["file"]).read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None
    macro = r["kind"].rsplit(":", 1)[-1]
    first = None
    for line in lines[int(r["line"]) - 1:]:
        m = re.match(r'\s*#include\s+"src/EFFECT/(\w+)\.h"', line)
        if m:
            hdr = f"EFFECT/{m[1]}.h"
            first = first or hdr
            if bodies is None or (hdr, macro) in bodies:
                return hdr
    return first


def template_rows(catalog: list[dict]) -> dict[str, dict]:
    """Mechanical names for effect template instances: Effects::Plg<NNN>::<template body name>, placed with the
    template body (Templates\\<Name>.c). The body is the template-body row of the header the binding feeds and the
    bound macro."""
    bodies = {(r["file"], r["name"]): r["final_name"].rsplit("::", 1)[-1] for r in catalog
              if r["kind"] == "template-body" and r["final_name"]}
    out = {}
    seen: collections.Counter = collections.Counter()
    rows = sorted((r for r in catalog if r["copy"] == "yes" and r["kind"].startswith("template:")),
                  key=lambda r: (r["overlay"], r["file"], int(r["line"] or 0)))
    # a binding is an instance only when the header it feeds defines that macro's function; other bindings
    # (PLG065 rebinds its particle functions so particleDispatch.h can call them) refer to that instance
    real = {}
    for r in rows:
        macro = r["kind"].rsplit(":", 1)[-1]
        if (template_header(r, bodies), macro) in bodies:
            real.setdefault((r["overlay"], r["name"]), r["id"])
    for r in rows:
        macro = r["kind"].rsplit(":", 1)[-1]  # VS_PARTICLE_RENDER_FUNCTION
        inst = real.get((r["overlay"], r["name"]))
        if inst and inst != r["id"]:
            continue
        tmpl = bodies.get((template_header(r, bodies), macro)) or "".join(
            w.capitalize() for w in macro.removeprefix("VS_").removesuffix("_FUNCTION").split("_"))
        plg = re.sub(r"\D", "", r["overlay"]) or r["overlay"]
        seen[(plg, tmpl)] += 1
        nth = seen[(plg, tmpl)]
        out[r["id"]] = {
            "final_name": f"Effects::Plg{plg}::{tmpl}{nth if nth > 1 else ''}",
            "final_location": f"Native\\Game\\src\\Effects\\Templates\\{tmpl}.c",
            "purpose": f"PLG{plg} instance of the shared {tmpl} effect template; its parameters become an effects-catalog row.",
            "confidence": "high", "notes": "template-instance",
        }
    for r in rows:
        inst = real.get((r["overlay"], r["name"]))
        if inst and inst != r["id"] and inst in out:
            out[r["id"]] = {**out[inst], "notes": f"template-binding;dup-of:{inst}"}
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--check")
    g.add_argument("--check-all", action="store_true")
    g.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    if args.check:
        errs, rows = check_batch(args.check)
        errs += check_unique({r["id"]: r for r in rows if r.get("final_name")}, template_rows(read_csv(CATALOG)))
        for e in errs[:200]:
            print(e)
        print(f"{args.check}: {len(rows)} rows, {len(errs)} errors")
        return 1 if errs else 0

    batches = sorted(p.stem for p in BATCHES.glob("*.csv"))
    all_errs: list[str] = []
    all_rows: dict[str, dict] = {}
    done = 0
    for b in batches:
        if not (OUTS / f"{b}.csv").exists():
            all_errs.append(f"{b}: output missing")
            continue
        errs, rows = check_batch(b)
        all_errs += errs
        done += 1
        for r in rows:
            all_rows[r["id"]] = r
    all_errs += check_unique(all_rows, template_rows(read_csv(CATALOG)))
    conf = collections.Counter(r.get("confidence", "").strip() for r in all_rows.values())
    print(f"{done}/{len(batches)} batch outputs, {len(all_rows)} rows, confidence {dict(conf)}")
    for e in all_errs[:300]:
        print(e)
    print(f"{len(all_errs)} errors")
    if args.check_all or all_errs:
        return 1 if all_errs else 0

    catalog = read_csv(CATALOG)
    cols = list(catalog[0].keys())
    templates = template_rows(catalog)
    named = {**all_rows, **templates}
    applied = 0
    for r in catalog:
        n = named.get(r["id"])
        if not n:
            continue
        r["final_name"] = n["final_name"].strip()
        r["final_location"] = n["final_location"].strip()
        r["purpose"] = n["purpose"].strip()
        tokens = [t for t in (r.get("notes") or "").split(";") if t and not t.startswith("conf:")]
        # dup-of comes from the batch output (or the template rule) alone, so a corrected target replaces the old one
        tokens = [t for t in tokens if not t.startswith("dup-of:")]
        if r["id"] in templates:
            # mechanical template names are recomputed every apply: drop the previous run's template tokens
            tokens = [t for t in tokens if not t.startswith(("template-instance", "template-binding"))]
        tokens.insert(0, f"conf:{n['confidence'].strip()}")
        extra = (n.get("notes") or "").strip()
        if extra and extra not in ";".join(tokens):
            tokens.append(extra)
        r["notes"] = ";".join(tokens)
        if re.search(r"(^|;)\s*is-data\b", extra):
            r["copy"] = "data"  # disassembly mistook a data table for a function; copied as a data definition
        if re.search(r"(^|;)\s*not-linked\b", extra):
            r["copy"] = "no"  # stale split file the overlay's splat.yaml does not build; the linked file has the code
        applied += 1
    with CATALOG.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        w.writerows(catalog)
    need = sum(1 for r in catalog if r["copy"] in ("yes", "data"))
    have = sum(1 for r in catalog if r["copy"] in ("yes", "data") and r["final_name"] and r["final_location"]
               and r["purpose"])
    print(f"applied {applied}; catalog coverage {have}/{need} "
          f"({sum(r['copy'] == 'data' for r in catalog)} reclassified as data)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
