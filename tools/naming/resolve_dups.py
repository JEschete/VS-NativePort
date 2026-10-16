"""Resolve cross-batch C-name collisions in the naming outputs.

Rows from different batches that were given the same final_name (agreed between agents) are compared by
normalized body: whitespace is collapsed and every called/referenced decomp function name is replaced by its
final_name (resolved inside the row's own overlay), so overlay-local spellings of the same callee compare equal.

  identical bodies -> later rows get `dup-of:<first id>` in tools/naming/out/<batch>.csv notes
                      (refused across overlays when the body touches mutable file-static state: a dup-of
                      collapses to one native copy, which would merge two overlays' private state)
  anything else    -> needs a row in tools/naming/dup_decisions.csv:
                      action=dup (behaviorally identical, spelling differs) or action=rename (new name/location)

Usage: python tools/naming/resolve_dups.py [--diff] [--write]   (exit 1 while undecided collisions remain)
"""
from __future__ import annotations

import argparse
import collections
import csv
import difflib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ledger"))
sys.path.insert(0, str(ROOT / "tools" / "naming"))
from build_ledgers import load_sdk  # noqa: E402
from merge_names import CATALOG, HEADER, OUTS, c_spelling, dup_target, read_csv  # noqa: E402

# Bodies come from the libclang index (tools/port/decomp_index.py); the regex scanner in build_ledgers mis-pairs
# bodies after `__attribute__((unused));` forward declarations.
INDEX = ROOT / "build" / "port" / "index.json"
DECOMP = ROOT / "resources" / "rood-reverseWin"
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
DECISIONS = ROOT / "tools" / "naming" / "dup_decisions.csv"

IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
TOK = re.compile(r"\$?\w+|\S")
KEYWORDS = {"if", "else", "while", "for", "do", "switch", "case", "default", "break", "continue", "return", "goto",
            "sizeof", "static", "const", "volatile", "unsigned", "signed", "char", "short", "int", "long", "void",
            "struct", "union", "enum", "register", "extern", "typedef", "float", "double"}
SDK_NAMES: set[str] = set()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--diff", action="store_true", help="print token diffs for differing bodies")
    a = ap.parse_args()

    catalog = {r["id"]: r for r in read_csv(CATALOG)}
    rows: dict[str, dict] = {}
    row_batch: dict[str, str] = {}
    for p in sorted(OUTS.glob("*.csv")):
        for r in read_csv(p):
            rows[r["id"]] = r
            row_batch[r["id"]] = p.stem

    SDK_NAMES.update(load_sdk())
    idx = json.loads(INDEX.read_text(encoding="utf-8"))
    src_cache: dict[Path, bytes] = {}

    def body_of(c: dict) -> str | None:
        f = idx_fn.get((c["file"], c["name"]))
        if not f:
            return None
        p = DECOMP / f["file"]
        text = src_cache.setdefault(p, p.read_bytes())[f["start"]:f["end"]].decode("utf-8", "replace")
        text = COMMENT.sub(" ", text)
        brace = text.find("{")
        return text[brace:] if brace >= 0 else None

    idx_fn = {(f["file"].removeprefix("src/"), f["name"]): f for f in idx["functions"].values()}
    body = {(c["file"], int(c["line"] or 0), c["name"]): body_of(c) for c in catalog.values()}
    final_by_ov = collections.defaultdict(dict)
    for rid, c in catalog.items():
        r = rows.get(rid)
        if r:
            final_by_ov[c["overlay"]][c["name"]] = c_spelling(r["final_name"].strip())
    # Identifiers with program-wide meaning keep their spelling; everything else (locals, params, static locals)
    # is canonicalized by order of first appearance.
    known = set(KEYWORDS)
    mutable_statics: set[str] = set()
    known |= set(idx["globals"])
    for n, g in idx["globals"].items():
        d = g["defs"] or g["decls"]
        if g["defs"] and all(x["static"] for x in g["defs"]) and not any("const" in x["type"] for x in d):
            mutable_statics.add(n)
    known |= set(idx["enum_consts"]) | set(idx["macros"]) | {f["name"] for f in idx["functions"].values()}
    known |= {t["key"].split(":", 1)[1] for t in idx["types"].values()}
    known |= {n for m in final_by_ov.values() for n in m.values()}

    def uses_mutable_static(rid: str) -> list[str]:
        c = catalog[rid]
        b = body.get((c["file"], int(c["line"] or 0), c["name"])) or ""
        return sorted({w for w in IDENT.findall(b) if w in mutable_statics})

    def norm(rid: str) -> str | None:
        c = catalog[rid]
        b = body.get((c["file"], int(c["line"] or 0), c["name"]))
        if b is None:
            return None
        names = final_by_ov[c["overlay"]]
        b = IDENT.sub(lambda m: names.get(m.group(0), m.group(0)), b)
        local: dict[str, str] = {}

        def canon(m: re.Match) -> str:
            w = m.group(0)
            if w in known or w in SDK_NAMES:
                return w
            return local.setdefault(w, f"$L{len(local)}")
        b = IDENT.sub(canon, b)
        return re.sub(r"\s+", " ", b).strip()

    groups = collections.defaultdict(list)
    for rid, r in rows.items():
        if not dup_target(r.get("notes", "")):
            groups[c_spelling(r["final_name"].strip())].append(rid)

    marks: dict[str, str] = {}
    manual = []
    for cname, ids in sorted(groups.items()):
        if len(ids) < 2:
            continue
        ids.sort()
        first = ids[0]
        ref = norm(first)
        for rid in ids[1:]:
            other = norm(rid)
            locs_same = rows[rid]["final_location"].strip() == rows[first]["final_location"].strip()
            if ref is not None and ref == other and locs_same:
                st = uses_mutable_static(rid) + uses_mutable_static(first)
                if st and catalog[rid]["overlay"] != catalog[first]["overlay"]:
                    manual.append((cname, first, rid, f"identical but uses mutable file-static {st[:3]}"))
                    continue
                marks[rid] = first
            else:
                why = "location differs" if not locs_same else ("no body (asm)" if other is None or ref is None
                                                                  else "body differs")
                manual.append((cname, first, rid, why))
                if a.diff and why == "body differs" and ref and other:
                    ta, tb = TOK.findall(ref), TOK.findall(other)
                    d = [x for x in difflib.ndiff(ta, tb) if x[0] in "+-"]
                    print(f"  DIFF {cname} {first}->{rid}: {' '.join(d)[:600]}")

    decisions = {d["id"]: d for d in read_csv(DECISIONS)} if DECISIONS.exists() else {}
    undecided = [m for m in manual if m[2] not in decisions]
    print(f"{len(marks)} rows identical -> dup-of; {len(manual)} need a decision "
          f"({len(manual) - len(undecided)} recorded in {DECISIONS.name})")
    for rid, first in sorted(marks.items()):
        print(f"  DUP {rows[rid]['final_name']} {rid} -> {first} ({catalog[rid]['overlay']} -> {catalog[first]['overlay']})")
    for m in undecided:
        print("  MANUAL", *m)
    if a.write and (marks or decisions):
        edits: dict[str, dict] = {}
        for rid, first in marks.items():
            edits[rid] = {"note": f"dup-of:{first}"}
        for rid, d in decisions.items():
            if d["action"] == "dup":
                edits[rid] = {"note": d["note"]}
            elif d["action"] == "rename":
                edits[rid] = {"note": d["note"], "final_name": d["final_name"], "final_location": d["final_location"]}
            else:
                raise SystemExit(f"{DECISIONS.name}:{rid}: unknown action {d['action']}")
        by_batch = collections.defaultdict(dict)
        for rid, e in edits.items():
            by_batch[row_batch[rid]][rid] = e
        for batch, m in by_batch.items():
            path = OUTS / f"{batch}.csv"
            out_rows = read_csv(path)
            for r in out_rows:
                e = m.get(r["id"])
                if not e:
                    continue
                note = (r.get("notes") or "").strip()
                if e["note"] not in note:
                    r["notes"] = (note + "; " if note else "") + e["note"]
                for k in ("final_name", "final_location"):
                    if k in e:
                        r[k] = e[k]
            with path.open("w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=HEADER, extrasaction="ignore")
                w.writeheader()
                w.writerows(out_rows)
        print(f"applied {len(edits)} edits across {len(by_batch)} batch outputs")
    return 1 if undecided else 0


if __name__ == "__main__":
    raise SystemExit(main())
