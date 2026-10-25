"""Deterministic naming evidence: exact source lines for every symbol in a batch (no model involved).

  python tools\\naming\\evidence.py <batch> [--globals | --types] [--ids G00012,T00199] [--max N] [--out FILE]

Globals: the storage identity's own definition (resolved through the decomp index, honouring file/local-static
scope) and up to N uses inside each function that references it (from build\\port\\symbol_context.json).
Types: the declaration body, then for each member up to N accesses `->member` / `.member`, first inside functions
whose libclang field references include that exact `Type.member` (so the member is known to belong to this type),
then inside the other functions that reference the type.

Every line printed is `file:line: text` copied from resources\\rood-reverseWin\\src, so names can be derived and
cited from it directly.

--upstream N adds up to N sentences per identifier from the cached upstream pull requests
(build\\port\\upstream_prs.json, see upstream_prs.py) that mention the original symbol, type or referencing
function by exact name. They are labelled `PR#n author:` (`[AI-declared]` when the PR says it was written with an
AI model) and are hints only: a name still needs a source line.
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
UPSTREAM_REF = ROOT / "resources" / "rood-reverse"
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


class UpstreamSource:
    """Member accesses in resources\\rood-reverse (upstream main) for fields whose layout was adopted from upstream
    (hint `layout:upstream`, see tools\\naming\\upstream_layout.py). Lines print as `upstream:<file>:<line>`."""

    def __init__(self) -> None:
        self.ix: dict | None = None
        self.cache: dict[str, list[str]] = {}

    def lines(self, rel: str) -> list[str]:
        if rel not in self.cache:
            p = UPSTREAM_REF / rel
            self.cache[rel] = p.read_text(encoding="utf-8", errors="replace").splitlines() if p.exists() else []
        return self.cache[rel]

    def show(self, rel: str, n: int) -> str:
        text = self.lines(rel)
        return f"upstream:{rel.removeprefix('src/')}:{n}: {text[n - 1].strip()[:140]}" if 0 < n <= len(text) else "?"

    def decl(self, type_name: str) -> str | None:
        self.load()
        for t in self.ix["types"].values():
            if t["key"].split(":", 1)[1] == type_name and t["fields"]:
                return self.show(t["file"], t["line"])
        return None

    def load(self) -> None:
        if self.ix is None:
            path = ROOT / "build" / "port" / "index_upstream.json"
            if not path.exists():
                sys.exit(f"{path} missing: run tools\\port\\decomp_index.py --root resources\\rood-reverse --out {path}")
            self.ix = json.loads(path.read_text(encoding="utf-8"))

    def hits(self, type_name: str, member: str, limit: int) -> list[str]:
        self.load()
        mpat = re.compile(r"(->|\.)" + re.escape(member) + r"\b")
        out = []
        for fn in self.ix["functions"].values():
            if f"{type_name}.{member}" not in (fn.get("refs") or {}).get("field", {}):
                continue
            text = self.lines(fn["file"])
            for i in range(fn["line"] - 1, min(fn["end_line"], len(text))):
                if mpat.search(text[i]):
                    out.append(f"[{fn['name']}] {self.show(fn['file'], i + 1)}")
                    if len(out) >= limit:
                        return out
        return out


class Upstream:
    """Exact-identifier sentence lookup over cached upstream PR titles, bodies and comments."""

    def __init__(self, limit: int) -> None:
        self.limit = limit
        self.seen: set[tuple[int, str]] = set()
        self.units: list[tuple[int, str, str]] = []
        path = ROOT / "build" / "port" / "upstream_prs.json"
        if limit and not path.exists():
            sys.exit(f"{path} missing: run tools\\naming\\upstream_prs.py first")
        if not limit:
            return
        for p in json.loads(path.read_text(encoding="utf-8")):
            ai = bool(re.search(r"\b(Claude|Copilot|GPT|ChatGPT|LLM|Codex|Gemini)\b", p["title"] + " " + p["body"]))
            texts = [(p["author"], p["title"]), (p["author"], p["body"])]
            texts += [(c["author"], c["body"]) for c in p.get("comments", [])]
            for author, text in texts:
                label = f"{author or '?'}{' [AI-declared]' if ai and author == p['author'] else ''}"
                for line in (text or "").splitlines():
                    for sent in re.split(r"(?<=[.;])\s+(?=[A-Z`])", line.strip(" -*#>")):
                        if sent:
                            self.units.append((p["number"], label, sent))

    def lines(self, ident: str) -> list[str]:
        if not self.limit or not ident or ident.startswith("("):
            return []
        pat = re.compile(r"(?<![\w])" + re.escape(ident) + r"(?![\w])")
        hits: list[str] = []
        for number, author, sent in self.units:
            if pat.search(sent) and (number, sent) not in self.seen:
                self.seen.add((number, sent))
                hits.append(f"PR#{number} {author}: {sent[:260]}")
                if len(hits) >= self.limit:
                    break
        return hits


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("batch")
    kind = ap.add_mutually_exclusive_group()
    kind.add_argument("--globals", action="store_true")
    kind.add_argument("--types", action="store_true")
    ap.add_argument("--ids", default="")
    ap.add_argument("--max", type=int, default=3)
    ap.add_argument("--out")
    ap.add_argument("--upstream", type=int, default=0)
    ap.add_argument("--funcs", type=int, default=0, help="limit referencing functions shown per global (0 = all)")
    a = ap.parse_args()
    up = Upstream(a.upstream)

    index = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))
    context = json.loads((ROOT / "build" / "port" / "symbol_context.json").read_text(encoding="utf-8"))
    funcs = {(f["file"], f["name"]): (f["line"], f["end_line"]) for f in index["functions"].values()}
    with (ROOT / "Docs" / "ledgers" / "DATA_LEDGER.csv").open(encoding="utf-8") as stream:
        data = {r["id"]: r for r in csv.DictReader(stream)}
    with (ROOT / "Docs" / "ledgers" / "TYPE_LEDGER.csv").open(encoding="utf-8") as stream:
        types = {r["id"]: r for r in csv.DictReader(stream)}
    with (ROOT / "Docs" / "ledgers" / "FIELD_LEDGER.csv").open(encoding="utf-8") as stream:
        fields = list(csv.DictReader(stream))
    only = {i.strip() for i in a.ids.split(",") if i.strip()}
    src = Source()
    upsrc = UpstreamSource()
    field_users: dict[str, list[tuple[str, str, int, int]]] = {}
    for fn in index["functions"].values():
        for key in (fn.get("refs") or {}).get("field", {}):
            field_users.setdefault(key, []).append((fn["name"], fn["file"], fn["line"], fn["end_line"]))
    out: list[str] = []

    def ref_functions(labels: list[str]):
        for label in labels:
            m = LABEL.search(label)
            if m:
                rel = f"src/{m.group(1)}"
                lo, hi = funcs.get((rel, m.group(3)), (int(m.group(2)), int(m.group(2))))
                yield label.split(" (")[0], m.group(3), rel, lo, hi

    if not a.types:
        with (BATCHES / a.batch / "globals.csv").open(encoding="utf-8") as stream:
            global_rows = list(csv.DictReader(stream))
        for row in global_rows:
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
            out.extend(f"   {h}" for h in up.lines(name))
            pat = re.compile(r"\b" + re.escape(name) + r"\b")
            labels = context["globals"].get(gid, [])
            if not labels and defs:
                labels = [f"? ({defs[0]['file'].removeprefix('src/')}:1 ?)"]
            shown = 0
            for final, orig, rel, lo, hi in ref_functions(labels):
                if a.funcs and shown >= a.funcs:
                    out.append(f"   ... {len(labels) - shown} more referencing functions")
                    break
                if orig == "?":
                    lo, hi = 1, len(src.lines(rel))
                hits = [i + 1 for i in range(lo - 1, min(hi, len(src.lines(rel))))
                        if pat.search(src.lines(rel)[i]) and not src.lines(rel)[i].lstrip().startswith("extern")]
                if hits:
                    shown += 1
                    out.append(f"   in {final} [{orig}]")
                    out.extend(f"      {src.show(rel, h)}" for h in hits[: a.max])
                    out.extend(f"      {h}" for h in up.lines(orig))
            out.append("")

    if not a.globals:
        members: dict[str, list[dict]] = {}
        for f in fields:
            members.setdefault(f["type_id"], []).append(f)
        with (BATCHES / a.batch / "types.csv").open(encoding="utf-8") as stream:
            type_rows = list(csv.DictReader(stream))
        for row in type_rows:
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
                    for i, source_line in enumerate(text):
                        if re.search(r"\}\s*" + re.escape(t["name"]) + r"\s*;", source_line) or re.search(
                                r"\b(struct|union|enum)\s+" + re.escape(t["name"]) + r"\s*\{", source_line):
                            decl = (f"src/{rel}", i + 1)
                            break
                    if decl:
                        break
            if decl:
                out.append(f"   DECL {src.show(*decl)}")
            elif t["files"].startswith("upstream:"):
                d = upsrc.decl(t["name"])
                if d:
                    out.append(f"   DECL {d}")
            out.extend(f"   {h}" for h in up.lines(t["name"]))
            labels = context["types"].get(tid, [])
            for _final, orig, _rel, _lo, _hi in ref_functions(labels):
                out.extend(f"   [{orig}] {h}" for h in up.lines(orig))
            for f in members.get(tid, []):
                mpat = re.compile(r"(->|\.)" + re.escape(f["name"]) + r"\b")
                hits: list[str] = []
                from_upstream = "layout:upstream" in f.get("hints", "") or t["files"].startswith("upstream:")
                names = {t["name"], *t.get("aliases", "").split()}
                names |= {f"{k} {n}" for n in list(names) if n.startswith("(unnamed") for k in ("struct", "union")}
                typed = [(orig, rel, lo, hi) for n in names for orig, rel, lo, hi in field_users.get(f"{n}.{f['name']}", [])]
                seen_fns = {(rel, orig) for orig, rel, lo, hi in typed}
                fallback = [(orig, rel, lo, hi) for _, orig, rel, lo, hi in ref_functions(labels) if (rel, orig) not in seen_fns]
                for orig, rel, lo, hi in ([] if from_upstream else typed + fallback):
                    text = src.lines(rel)
                    for i in range(lo - 1, min(hi, len(text))):
                        if mpat.search(text[i]):
                            hits.append(f"[{orig}] {src.show(rel, i + 1)}")
                            if len(hits) >= a.max:
                                break
                    if len(hits) >= a.max:
                        break
                if from_upstream:
                    hits = upsrc.hits(t["name"], f["name"], a.max)
                desc = f"{f['type']}" + (f":{f['bits']}" if f["bits"] else "") + (f"={f['value']}" if f["value"] else "")
                tag = "  [layout from upstream]" if from_upstream else ""
                out.append(f"   .{f['name']} ({f['id']}, {desc}){tag}" + ("" if hits else "  -- no access in referencing functions"))
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
