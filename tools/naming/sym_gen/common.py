"""Shared machinery for the deterministic per-batch name generators (tools\\naming\\sym_gen\\<Batch>.py).

Names are written by hand in the batch script; this module only resolves citations from exact source text, so no
line number is ever typed:

  g = Gen("Game_Actors_01")
  g.c(("44F14.c", "scratch->unk3A = actor->unk0.unk14;"))       # source:44F14.c:849
  g.c(("upstream:5BF94.c", "func_800D52A4_t* motion"))         # source:upstream:BATTLE/BATTLE.PRG/5BF94.c:6728
  g.T(tid, role, final, location, purpose, conf, notes, [(orig, final, meaning, conf[, cite]), ...], recv=r"scratch")

Files are named by basename when it is unique in that tree, otherwise by path under src\\ (e.g.
`BATTLE/INITBTL.PRG/18.c`). A field without an explicit citation is cited from its first access through `recv`
inside a function that references the type (upstream functions for fields with the hint `layout:upstream`), else
from its declaration, which caps its confidence at `med`. Every field must cite a line that names its original
member; `write()` refuses otherwise.
"""
from __future__ import annotations

import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TREES = {"": ROOT / "resources" / "rood-reverseWin" / "src", "upstream:": ROOT / "resources" / "rood-reverse" / "src"}
INDEX = {"": ROOT / "build" / "port" / "index.json", "upstream:": ROOT / "build" / "port" / "index_upstream.json"}
LEDGERS = ROOT / "Docs" / "ledgers"
LABEL = re.compile(r"\(([^:()]+):(\d+) (\w+)\)$")


class Tree:
    def __init__(self, prefix: str) -> None:
        self.prefix, self.root = prefix, TREES[prefix]
        self.by_base: dict[str, list[str]] = {}
        for p in self.root.rglob("*"):
            if p.suffix in (".c", ".h"):
                self.by_base.setdefault(p.name, []).append(p.relative_to(self.root).as_posix())
        self.cache: dict[str, list[str]] = {}
        self._ix: dict | None = None

    @property
    def ix(self) -> dict:
        if self._ix is None:
            self._ix = json.loads(INDEX[self.prefix].read_text(encoding="utf-8"))
        return self._ix

    def rel(self, name: str) -> str:
        name = name.replace("\\", "/").removeprefix("src/")
        if "/" in name:
            return name
        hits = self.by_base.get(name, [])
        if len(hits) != 1:
            sys.exit(f"{self.prefix}{name}: {len(hits)} files with that name; use the path under src/")
        return hits[0]

    def label(self, rel: str) -> str:
        base = rel.rsplit("/", 1)[-1]
        short = base if len(self.by_base.get(base, [])) == 1 else rel
        return f"source:{self.prefix}{short}"

    def lines(self, rel: str) -> list[str]:
        if rel not in self.cache:
            self.cache[rel] = (self.root / rel).read_text(encoding="utf-8", errors="replace").splitlines()
        return self.cache[rel]


class Gen:
    def __init__(self, batch: str) -> None:
        self.batch = batch
        self.trees = {p: Tree(p) for p in TREES}
        self.types = {r["id"]: r for r in csv.DictReader(open(LEDGERS / "TYPE_LEDGER.csv", encoding="utf-8"))}
        self.members: dict[str, list[dict]] = {}
        for r in csv.DictReader(open(LEDGERS / "FIELD_LEDGER.csv", encoding="utf-8")):
            self.members.setdefault(r["type_id"], []).append(r)
        self.context = json.loads((ROOT / "build" / "port" / "symbol_context.json").read_text(encoding="utf-8"))
        self.TYPES: list[list[str]] = []
        self.FIELDS: list[list[str]] = []
        self.GLOBALS: list[list[str]] = []

    # ------------------------------------------------------------------ citations
    def split(self, f: str) -> tuple[Tree, str]:
        prefix = "upstream:" if f.startswith("upstream:") else ""
        t = self.trees[prefix]
        return t, t.rel(f.removeprefix(prefix))

    def cite(self, f: str, text: str, nth: int = 1) -> str:
        t, rel = self.split(f)
        seen = 0
        for i, line in enumerate(t.lines(rel)):
            if text in line:
                seen += 1
                if seen == nth:
                    return f"{t.label(rel)}:{i + 1}"
        sys.exit(f"citation text not found in {f}: {text!r}")

    def c(self, *pairs) -> str:
        return ";".join(self.cite(*p) for p in pairs)

    def resolve(self, citation: str) -> str:
        m = re.fullmatch(r"source:(upstream:)?(.+):(\d+)", citation)
        t = self.trees[m.group(1) or ""]
        return t.lines(t.rel(m.group(2)))[int(m.group(3)) - 1]

    # ------------------------------------------------------------------ automatic citations
    def ref_functions(self, tid: str, tree: Tree, type_name: str, member: str):
        """Functions whose libclang field references include type.member (any alias of the type), then the
        functions recorded as referencing the type."""
        names = {type_name, *self.types[tid].get("aliases", "").split()} if tid in self.types else {type_name}
        names |= {f"{k} {n}" for n in list(names) if n.startswith("(unnamed") for k in ("struct", "union")}
        seen = set()
        for fn in tree.ix["functions"].values():
            fields = (fn.get("refs") or {}).get("field", {})
            if any(f"{n}.{member}" in fields for n in names):
                seen.add((fn["file"], fn["name"]))
                yield fn["file"].removeprefix("src/"), fn["line"], fn["end_line"]
        if tree.prefix:
            return
        funcs = {(f["file"], f["name"]): (f["line"], f["end_line"]) for f in tree.ix["functions"].values()}
        for label in self.context["types"].get(tid, []):
            m = LABEL.search(label)
            if m and (f"src/{m.group(1)}", m.group(3)) not in seen:
                lo, hi = funcs.get((f"src/{m.group(1)}", m.group(3)), (int(m.group(2)), int(m.group(2))))
                yield m.group(1), lo, hi

    def use(self, tid: str, member: str, recv: str, upstream: bool = False) -> str | None:
        tree = self.trees["upstream:" if upstream else ""]
        rx = re.compile(r"(?:" + recv + r")\s*(?:->|\.)\s*" + re.escape(member) + r"(?!\w)")
        for rel, lo, hi in self.ref_functions(tid, tree, self.types[tid]["name"], member):
            lines = tree.lines(rel)
            for i in range(lo - 1, min(hi, len(lines))):
                if rx.search(lines[i]):
                    return f"{tree.label(rel)}:{i + 1}"
        return None

    def decl(self, tid: str, member: str, upstream: bool = False) -> str:
        tree = self.trees["upstream:" if upstream else ""]
        name = self.types[tid]["name"]
        rx = re.compile(r"(?<!\w)" + re.escape(member) + r"(?!\w)")
        for t in tree.ix["types"].values():
            if t["key"].split(":", 1)[1] != name or not any(x["name"] == member for x in t["fields"]):
                continue
            rel = t["file"].removeprefix("src/")
            lines = tree.lines(rel)
            for i in range(t["line"] - 1, min(t["end_line"], len(lines))):
                if rx.search(lines[i]):
                    return f"{tree.label(rel)}:{i + 1}"
        if not upstream:
            return self.decl(tid, member, True)
        sys.exit(f"{tid} {name}: declaration of {member} not found")

    # ------------------------------------------------------------------ rows
    def G(self, gid, disp, name, loc, purpose, conf, notes) -> None:
        self.GLOBALS.append([gid, disp, name, loc, purpose, conf, notes])

    def D(self, canonical: str, duplicates: list[str], why: str = "identical layout indexed through a different include path") -> None:
        """Duplicate type rows: same final name, location and field names as `canonical` (already written by T())."""
        trow = next(t for t in self.TYPES if t[0] == canonical)
        cfields = [f for f in self.FIELDS if f[0].startswith(canonical + ".")]
        for tid in duplicates:
            if len(self.members[tid]) != len(cfields):
                sys.exit(f"{tid}: {len(self.members[tid])} members, canonical {canonical} has {len(cfields)}")
            corig = [m["name"] for m in self.members[canonical]]
            if [m["name"] for m in self.members[tid]] != corig and "different member spellings" not in why:
                sys.exit(f"{tid}: member names differ from {canonical}; pass why='... different member spellings ...'")
            self.TYPES.append([tid, trow[1], trow[2], trow[3], trow[4], trow[5], f"same-as:{canonical};{why};{trow[6]}"])
            for m, cf in zip(self.members[tid], cfields):
                self.FIELDS.append([m["id"], cf[1], cf[2], cf[3], cf[4]])

    def applied(self, tid: str) -> tuple[dict, list[dict]]:
        """An already-applied type row and its field rows (to keep a canonical consistent with a named twin)."""
        return self.types[tid], self.members[tid]

    # ------------------------------------------------------------------ generic member naming
    PLACEHOLDER = re.compile(r"^(unk|pad|field|f|flag|flags|b|bits|bytes|raw|low|high|value|rest|gap|prefix|padding)"
                             r"[0-9A-Fa-f_]*$|^unk")

    def field_refs(self) -> set[str]:
        if not hasattr(self, "_refs"):
            self._refs = {k for p in ("", "upstream:") for fn in self.trees[p].ix["functions"].values()
                          for k in (fn.get("refs") or {}).get("field", {})}
        return self._refs

    def is_used(self, tid: str, member: str) -> bool:
        names = {self.types[tid]["name"], *self.types[tid].get("aliases", "").split()}
        names |= {f"{k} {n}" for n in list(names) if n.startswith("(unnamed") for k in ("struct", "union")}
        return any(f"{n}.{member}" in self.field_refs() for n in names)

    def layout(self, tid: str) -> dict:
        """Index layout matching the ledger's member list: pinned first, then upstream (adopted merged layouts)."""
        key = self.types[tid]["key"].partition("#")[0]
        want = [m["name"] for m in self.members[tid]]
        for p in ("", "upstream:"):
            for x in self.trees[p].ix["types"].values():
                if x["key"] == key and [f["name"] for f in x["fields"]] == want:
                    return x
        for p in ("", "upstream:"):
            for x in self.trees[p].ix["types"].values():
                if x["key"] == key and len(x["fields"]) == len(want):
                    return x
        # merged layout (UPSTREAM_LAYOUTS): each member's offset from the pinned layout by name, else upstream's
        sources = [x for p in ("", "upstream:") for x in self.trees[p].ix["types"].values() if x["key"] == key]
        fields = []
        for n in want:
            f = next((f for x in sources for f in x["fields"] if f["name"] == n), None)
            if f is None:
                raise KeyError(f"{tid}: no index layout with {len(want)} members and no member {n!r} in any layout")
            fields.append(f)
        if sources:
            return {"key": key, "size": sources[0]["size"], "fields": fields}
        raise KeyError(f"{tid}: no index layout with {len(want)} members")

    def member_offsets(self, tid: str) -> list[int]:
        """Bit offset of each member."""
        return [f["offset"] if f["offset"] is not None and f["offset"] >= 0 else 0 for f in self.layout(tid)["fields"]]

    def generic_specs(self, tid: str, overrides: dict | None = None) -> list[tuple]:
        """Used member: keep a meaningful fork name (med) or opaque<Off> (low); unread member: reserved<Off> (low)."""
        overrides = overrides or {}
        out, seen = [], set()
        for m, bit in zip(self.members[tid], self.member_offsets(tid)):
            n = m["name"]
            off = f"{bit // 8:X}" + (f"Bit{bit % 8}" if m.get("bits") else "")
            if n in overrides:
                spec = (n, *overrides[n])
            elif not self.is_used(tid, n):
                spec = (n, f"reserved{off}", "Not read by decompiled code", "low")
            elif self.PLACEHOLDER.search(n):
                spec = (n, f"opaque{off}", "Used, but its meaning is not established", "low")
            else:
                spec = (n, n[0].lower() + n[1:], "Named in the fork; see cited use", "med")
            if spec[1] in seen:
                spec = (spec[0], f"{spec[1]}{off}", *spec[2:])
            seen.add(spec[1])
            out.append(spec)
        return out

    def names_from_views(self, tid: str, sources: list[str]) -> dict:
        """Overrides for generic_specs: canonical members take the applied (or already generated) name of a view
        member with the same bit offset and width, unless that name is reserved/opaque."""
        named = {f["id"]: (f["final_name"], f["meaning"], f["confidence"])
                 for ms in self.members.values() for f in ms if f.get("final_name")}
        named.update({f[0]: (f[1], f[2], f[3]) for f in self.FIELDS})

        def spans(t):
            lay, ms = self.layout(t), self.members[t]
            offs = [f["offset"] for f in lay["fields"]]
            out = []
            for i, (m, f) in enumerate(zip(ms, lay["fields"])):
                width = int(m["bits"]) if m.get("bits") else (
                    ((offs[i + 1] if i + 1 < len(offs) else lay["size"] * 8) - offs[i]))
                out.append(((f["offset"], width), m))
            return out
        src = {}
        for s in sources:
            for key, m in spans(s):
                n = named.get(m["id"])
                if n and not n[0].startswith(("reserved", "opaque")):
                    src.setdefault(key, (n[0], f"{n[1]} (same bits as {s} {m['name']})", n[2]))
        return {m["name"]: src[key] for key, m in spans(tid) if key in src}

    def T(self, tid, role, name, loc, purpose, conf, notes, specs, recv: str | None = None) -> None:
        """specs: (orig, final, meaning, conf[, cite]); cite None -> first use through recv, else declaration-only."""
        self.TYPES.append([tid, role, name, loc, purpose, conf, notes])
        order = self.members[tid]
        if [o["name"] for o in order] != [sp[0] for sp in specs]:
            sys.exit(f"{tid}: member order\n  ledger {[o['name'] for o in order]}\n  specs  {[sp[0] for sp in specs]}")
        for o, sp in zip(order, specs):
            orig, final, meaning, fconf = sp[:4]
            cite_ = sp[4] if len(sp) > 4 else None
            upstream = "layout:upstream" in o.get("hints", "") or self.types[tid]["files"].startswith("upstream:")
            if cite_ is None and recv:
                cite_ = self.use(tid, orig, recv, upstream)
            if cite_ is None:
                cite_ = "declaration-only;" + self.decl(tid, orig, upstream)
                if fconf == "high":
                    fconf = "med"
            self.FIELDS.append([o["id"], final, meaning, fconf, cite_])

    # ------------------------------------------------------------------ output
    def write(self, partial: bool = False) -> None:
        problems = []
        orig_of = {m["id"]: m["name"] for ms in self.members.values() for m in ms}
        dup_types = {t[0] for t in self.TYPES if t[6].startswith("same-as:")}
        for fid, final, meaning, conf, notes in self.FIELDS:
            if fid.split(".")[0] in dup_types:
                continue
            orig = orig_of[fid]
            cites = re.findall(r"source:(?:upstream:)?[^:;]+:\d+", notes)
            if not any(re.search(r"(?<!\w)" + re.escape(orig) + r"(?!\w)", self.resolve(x)) for x in cites):
                problems.append(f"{fid} {orig}: no cited line names the member ({notes})")
        if problems:
            sys.exit("\n".join(problems))
        batch_dir = ROOT / "tools" / "naming" / "sym_batches" / self.batch
        out = ROOT / "tools" / "naming" / "sym_out" / self.batch
        out.mkdir(parents=True, exist_ok=True)
        torder = [r["id"] for r in csv.DictReader(open(batch_dir / "types.csv", encoding="utf-8"))]
        gorder = [r["id"] for r in csv.DictReader(open(batch_dir / "globals.csv", encoding="utf-8"))]
        tby, gby = {t[0]: t for t in self.TYPES}, {g[0]: g for g in self.GLOBALS}
        fby = {f[0]: f for f in self.FIELDS}
        forder = [m["id"] for tid in torder for m in self.members.get(tid, [])]
        for name, cols, order, by in (
                ("types.csv", ["id", "role", "final_name", "final_location", "purpose", "confidence", "notes"], torder, tby),
                ("fields.csv", ["id", "final_name", "meaning", "confidence", "notes"], forder, fby),
                ("globals.csv", ["id", "disposition", "final_name", "final_location", "purpose", "confidence", "notes"],
                 gorder, gby)):
            with open(out / name, "w", newline="", encoding="utf-8") as f:
                w = csv.writer(f, lineterminator="\n")
                w.writerow(cols)
                w.writerows(by[i] for i in order if i in by)
        missing_t = [i for i in torder if i not in tby]
        missing_g = [i for i in gorder if i not in gby]
        print(f"{self.batch}: {len(self.TYPES)}/{len(torder)} types, {len(self.FIELDS)}/{len(forder)} fields, "
              f"{len(self.GLOBALS)}/{len(gorder)} globals")
        if (missing_t or missing_g) and not partial:
            sys.exit(f"missing types {missing_t[:10]} globals {missing_g[:10]}")
