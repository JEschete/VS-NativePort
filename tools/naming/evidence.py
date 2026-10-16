"""Deterministic naming evidence: exact source lines for every symbol in a batch (no model involved).

  python tools\\naming\\evidence.py <batch> [--globals | --types] [--ids G00012,T00199] [--max N] [--out FILE]

Globals: the storage identity's own definition (resolved through the decomp index, honouring file/local-static
scope) and up to N uses inside each function that references it (from build\\port\\symbol_context.json).
Types: the declaration body, then for each member up to N accesses `->member` / `.member` found inside the
functions that reference the type.

Every line printed is `file:line: text` copied from resources\\rood-reverseWin\\src, so names can be derived and
cited from it directly.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / "resources" / "rood-reverseWin"
BATCHES = ROOT / "tools" / "naming" / "sym_batches"
ANON = re.compile(r"\(unnamed at (?:.*[\\/])?src[\\/](.+?):(\d+):\d+\)")
LABEL = re.compile(r"\(([^:()]+):(\d+) (\w+)\)$")


class Source:
    def __init__(self) -> None:
        self.cache: dict[str, list[str]] = {}

    def lines(self, rel: str) -> list[str]:
        if rel not in self.cache:
            p = REF / rel
            self.cache[rel] = p.read_text(encoding="utf-8", errors="replace").splitlines() if p.exists() else []
        return self.cache[rel]

    def show(self, rel: str, n: int) -> str:
        text = self.lines(rel)
        return f"{Path(rel).name}:{n}: {text[n - 1].strip()[:140]}" if 0 < n <= len(text) else f"{rel}:{n}: ?"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("batch")
    kind = ap.add_mutually_exclusive_group()
    kind.add_argument("--globals", action="store_true")
    kind.add_argument("--types", action="store_true")
    ap.add_argument("--ids", default="")
    ap.add_argument("--max", type=int, default=3)
    ap.add_argument("--out")
    a = ap.parse_args()

    index = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))
    context = json.loads((ROOT / "build" / "port" / "symbol_context.json").read_text(encoding="utf-8"))
    funcs = {(f["file"], f["name"]): (f["line"], f["end_line"]) for f in index["functions"].values()}
    data = {r["id"]: r for r in csv.DictReader(open(ROOT / "Docs" / "ledgers" / "DATA_LEDGER.csv", encoding="utf-8"))}
    types = {r["id"]: r for r in csv.DictReader(open(ROOT / "Docs" / "ledgers" / "TYPE_LEDGER.csv", encoding="utf-8"))}
    fields = [r for r in csv.DictReader(open(ROOT / "Docs" / "ledgers" / "FIELD_LEDGER.csv", encoding="utf-8"))]
    only = {i.strip() for i in a.ids.split(",") if i.strip()}
    src = Source()
    out: list[str] = []

    def ref_functions(labels: list[str]):
        for label in labels:
            m = LABEL.search(label)
            if m:
                rel = f"src/{m.group(1)}"
                lo, hi = funcs.get((rel, m.group(3)), (int(m.group(2)), int(m.group(2))))
                yield label.split(" (")[0], m.group(3), rel, lo, hi

    if not a.types:
        for row in csv.DictReader(open(BATCHES / a.batch / "globals.csv", encoding="utf-8")):
            gid = row["id"]
            if only and gid not in only:
                continue
            d = data[gid]
            name = d["name"]
            where = d["key"].split("@", 1)[1]
            path, _, scope = where.partition(":") if where.startswith("src/") else ("", "", "")
            entry = index["globals"].get(name, {})
            defs = [c for c in entry.get("defs", []) + entry.get("decls", [])
                    if (not path or c["file"] == path) and (not scope or c.get("scope") == scope)]
            out.append(f"## {gid} {name} : {d['type']} ({d['linkage']}, {d['segment']}, size {d['size']})")
            for c in defs[:2]:
                out.append(f"   DEF {src.show(c['file'], c['line'])}")
            pat = re.compile(r"\b" + re.escape(name) + r"\b")
            labels = context["globals"].get(gid, [])
            if not labels and defs:
                labels = [f"? ({defs[0]['file'].removeprefix('src/')}:1 ?)"]
            for final, orig, rel, lo, hi in ref_functions(labels):
                if orig == "?":
                    lo, hi = 1, len(src.lines(rel))
                hits = [i + 1 for i in range(lo - 1, min(hi, len(src.lines(rel))))
                        if pat.search(src.lines(rel)[i]) and not src.lines(rel)[i].lstrip().startswith("extern")]
                if hits:
                    out.append(f"   in {final} [{orig}]")
                    out.extend(f"      {src.show(rel, h)}" for h in hits[: a.max])
            out.append("")

    if not a.globals:
        members: dict[str, list[dict]] = {}
        for f in fields:
            members.setdefault(f["type_id"], []).append(f)
        for row in csv.DictReader(open(BATCHES / a.batch / "types.csv", encoding="utf-8")):
            tid = row["id"]
            if only and tid not in only:
                continue
            t = types[tid]
            out.append(f"## {tid} {t['name']} size {t['size']}")
            m = ANON.search(t["name"])
            decl = None
            if m:
                rel = "src/" + m.group(1).replace("\\", "/")
                rel = str(Path(rel).as_posix())
                decl = (re.sub(r"[^/]+/\.\./", "", rel), int(m.group(2)))
            else:
                for rel in t["files"].split():
                    text = src.lines(f"src/{rel}")
                    for i, l in enumerate(text):
                        if re.search(r"\}\s*" + re.escape(t["name"]) + r"\s*;", l) or re.search(
                                r"\b(struct|union|enum)\s+" + re.escape(t["name"]) + r"\s*\{", l):
                            decl = (f"src/{rel}", i + 1)
                            break
                    if decl:
                        break
            if decl:
                out.append(f"   DECL {src.show(*decl)}")
            labels = context["types"].get(tid, [])
            for f in members.get(tid, []):
                mpat = re.compile(r"(->|\.)" + re.escape(f["name"]) + r"\b")
                hits: list[str] = []
                for final, orig, rel, lo, hi in ref_functions(labels):
                    text = src.lines(rel)
                    for i in range(lo - 1, min(hi, len(text))):
                        if mpat.search(text[i]):
                            hits.append(f"[{orig}] {src.show(rel, i + 1)}")
                            if len(hits) >= a.max:
                                break
                    if len(hits) >= a.max:
                        break
                desc = f"{f['type']}" + (f":{f['bits']}" if f["bits"] else "") + (f"={f['value']}" if f["value"] else "")
                out.append(f"   .{f['name']} ({f['id']}, {desc})" + ("" if hits else "  -- no access in referencing functions"))
                out.extend(f"      {h}" for h in hits)
            out.append("")

    text = "\n".join(out)
    if a.out:
        Path(a.out).write_text(text, encoding="utf-8")
        print(f"{a.out}: {len(out)} lines")
    else:
        sys.stdout.write(text + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
