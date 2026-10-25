"""Occurrence-level reference index: every identifier in game code resolved to its ledger row.

The copy tool renames by position, not by text, so a member such as `unk4` is renamed only where it belongs to the
right type. This walks every game translation unit with the same libclang setup as decomp_index.py and records,
per source file, each occurrence (declaration or use) of:

  functions   -> F##### (FUNCTION_CATALOG.csv), or sdk:<name> for PS1 SDK functions (also when a game file declares
                 the SDK function itself), or plugin:<name> for effect-plugin entry points that share one load
                 address (the effect catalog says which plugin is loaded)
  variables   -> G##### (DATA_LEDGER.csv); static storage only (locals and parameters are not renamed)
  types       -> T##### (TYPE_LEDGER.csv; typedef aliases fold into their struct)
  fields      -> T#####.NNN (FIELD_LEDGER.csv); for structs whose ledger layout was merged with upstream, a member
                 path "T#####.NNN>T#####.NNN" or "T#####.NNN>sdk:SVECTOR.vy" by bit offset, with a "~w<bits>" suffix
                 when the pinned member is a wider untyped view starting at that member (char[4] over a short + pad)
  enumerators -> T#####.NNN
  unresolved  -> ?<kind>:<name> (none in a full run)

Output build/port/refs.json (git-ignored):
  {"files": {"src/...c": [[line, col, length, kind, target], ...]}, "stats": {...}, "unresolved": [...]}
`col` is the 1-based byte column of the identifier's first character; the copy tool must check that the text at
(line, col, length) equals the original spelling before replacing it. Occurrences produced inside a macro body are
recorded at the macro definition's text (spelling location) with kind suffix "@macro".

A header occurrence seen from several translation units keeps every distinct target (e.g. a per-instance header
static resolves to a different G row per overlay); each target is "<target>|<overlay>" in that case.

The 22 translation units libclang reports errors for are old-compiler leniency in the decomp (an `extern struct X
a[]` of a not-yet-defined struct in 58EC.h, and functions called before their definition, which C89 declares
implicitly). Calls through an implicit declaration resolve to the definition with the same name in the same
translation unit; the stats report any occurrence in an error TU that still does not resolve.

  python tools\\port\\ref_index.py [--only <substring>] [--out <json>]
"""
from __future__ import annotations

import argparse
import collections
import csv
import ctypes
import hashlib
import json
import os
import re
import sys
import time
from pathlib import Path

import clang.cindex as ci

sys.path.insert(0, str(Path(__file__).resolve().parent))
import decomp_index as dx  # noqa: E402

REPO = dx.REPO
LEDGERS = REPO / "Docs" / "ledgers"
OUT = REPO / "build" / "port" / "refs.json"
K = ci.CursorKind
RESIDENT = ("SLUS_010.40", "BATTLE/BATTLE.PRG")
# Names the decomp defines only at link time (Makefile --defsym) for byte-matching, mapped to the function they are.
LINK_ALIASES = {
    # config/MENU/MENU9.PRG/Makefile: --defsym=_toVsStringPercentAlias=0x80106D5C. In the shipped MENU9.PRG that
    # address is a function start (after `jr ra`, opens a 0x18 stack frame, calls sprintf with "%d") and all four
    # percent call sites in _renderBossRushDetails jump to it: it is _toVsStringPercent.
    "_toVsStringPercentAlias": "_toVsStringPercent",
}


def overlay_of(rel: str) -> str:
    """src/BATTLE/BATTLE.PRG/146C.c -> BATTLE/BATTLE.PRG; src/SLUS_010.40/main.c -> SLUS_010.40."""
    p = rel.removeprefix("src/").split("/")
    return p[0] if p[0] == "SLUS_010.40" else "/".join(p[:2])


def spelling_location(loc: ci.SourceLocation) -> tuple[str | None, int, int]:
    """File, line and column where the token's text is written (inside a macro definition for macro bodies)."""
    lib = ci.conf.lib
    if lib.clang_getSpellingLocation.argtypes is None:
        lib.clang_getSpellingLocation.argtypes = [ci.SourceLocation, ctypes.POINTER(ctypes.c_void_p)] + \
            [ctypes.POINTER(ctypes.c_uint)] * 3
        lib.clang_getSpellingLocation.restype = None
    f, line, col, off = ctypes.c_void_p(), ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
    lib.clang_getSpellingLocation(loc, ctypes.byref(f), ctypes.byref(line), ctypes.byref(col), ctypes.byref(off))
    name = ci.File(ctypes.cast(f.value, ci.c_object_p)).name if f.value else None
    return name, line.value, col.value


UNNAMED = re.compile(r"\(unnamed at (.+?):(\d+):(\d+)\)")


def norm_key(key: str) -> str:
    """Unnamed types are keyed by the include path libclang used ("...\\BATTLE.PRG\\..\\..\\SLUS_010.40\\main.h:166:5"),
    which differs per translation unit; normalise the path so every TU agrees."""
    return UNNAMED.sub(lambda m: f"(unnamed at {os.path.normcase(os.path.normpath(m[1]))}:{m[2]}:{m[3]})", key)


# Modules resident while a module runs, after the module itself: MENUx overlays run with MAINMENU loaded, and
# everything runs with BATTLE.PRG and the executable resident.
def module_order(tu_rel: str) -> list[str]:
    own = overlay_of(tu_rel).split("/")[-1]
    order = [own]
    if tu_rel.startswith("src/MENU/") and own != "MAINMENU.PRG":
        order.append("MAINMENU.PRG")
    order += ["BATTLE.PRG", "SLUS_010.40"]
    return order


class Ledgers:
    def __init__(self, index: dict) -> None:
        rd = lambda n: list(csv.DictReader(open(LEDGERS / n, encoding="utf-8")))  # noqa: E731
        # rows from split files the overlay's splat.yaml does not build (merge_names `not-linked`) are not code
        cat = [r for r in rd("FUNCTION_CATALOG.csv") if "not-linked" not in r["notes"].split(";")]
        self.cat_by_file = {(r["file"], r["name"]): r["id"] for r in cat}
        self.cat_by_ov = collections.defaultdict(list)
        self.cat_by_name = collections.defaultdict(list)
        for r in cat:
            self.cat_by_ov[(r["overlay"], r["name"])].append(r["id"])
            self.cat_by_name[r["name"]].append(r["id"])
        self.cat_ov = {r["id"]: r["overlay"] for r in cat}
        self.cat_final = {r["id"]: r["final_name"] for r in cat}
        self.data = rd("DATA_LEDGER.csv")
        self.g_by_key = {r["key"]: r["id"] for r in self.data}
        self.g_by_name = collections.defaultdict(list)
        for r in self.data:
            self.g_by_name[r["name"]].append(r)
        types = rd("TYPE_LEDGER.csv")
        self.t_by_key = {norm_key(r["key"]): r["id"] for r in types}
        self.t_alias = {}
        for r in types:
            for a in r["aliases"].split():
                self.t_alias[f"typedef:{a}"] = norm_key(r["key"].partition("#")[0])
        self.multi = {k.partition("#")[0] for k in self.t_by_key if "#" in k}
        self.fields = collections.defaultdict(dict)
        for r in rd("FIELD_LEDGER.csv"):
            self.fields[r["type_id"]].setdefault(r["name"], r["id"])
        # layout hash of each indexed declaration, as build_symbol_ledgers.py computes it
        self.layout = {}
        for t in index["types"].values():
            sig = json.dumps([t.get("size"), [[x.get("name"), x.get("type"), x.get("bits"), x.get("value")]
                                              for x in t["fields"]]])
            self.layout[(norm_key(t["key"]), t["file"], t["line"])] = hashlib.sha1(sig.encode()).hexdigest()[:8]
        # address -> symbol per module, from the splat symbol files (keyed like the catalog's overlay column)
        self.symbols = {}
        cfg = dx.DECOMP / "config"
        for p in cfg.rglob("symbol_addrs.txt"):
            ov = p.parent.name
            for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
                m = re.match(r"\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)", line)
                if m:
                    self.symbols.setdefault((ov, int(m[2], 16)), m[1])
        sys.path.insert(0, str(REPO / "tools" / "ledger"))
        import build_ledgers
        self.sdk = set(build_ledgers.load_sdk())
        sys.path.insert(0, str(REPO / "tools" / "naming" / "sym_gen"))
        from common import Gen
        self.gen = Gen("Shared")
        self._layouts: dict = {}
        self.t_by_name = {}
        self.t_by_name_multi = collections.defaultdict(list)
        for r in types:
            names = [r["name"], *r["aliases"].split()]
            if "#" in r["key"]:
                for n in names:
                    self.t_by_name_multi[n].append(r["id"])
                continue
            for n in names:
                self.t_by_name.setdefault(n, r["id"])

    def type_id(self, d: ci.Cursor) -> str | None:
        key = norm_key(dx.type_key(d))
        key = self.t_alias.get(key, key)
        if key.startswith("typedef:") and key not in self.t_by_key:
            # typedef of an anonymous struct: the struct row carries the typedef as alias
            u = d.underlying_typedef_type.get_declaration() if d.kind == K.TYPEDEF_DECL else None
            if u is not None and u.kind in dx.TYPE_DECLS:
                return self.type_id(u)
        if key in self.multi:
            if d.kind == K.TYPEDEF_DECL:  # the layout hash belongs to the struct the typedef names
                u = d.underlying_typedef_type.get_declaration()
                if u is not None and u.kind in dx.TYPE_DECLS and u.kind != K.TYPEDEF_DECL:
                    d = u
            f, line = dx.loc(d)
            h = self.layout.get((key, f, line))
            return self.t_by_key.get(f"{key}#{h}") if h else None
        return self.t_by_key.get(key)

    def global_id(self, v: ci.Cursor, tu_rel: str) -> str | None:
        name = v.spelling
        f, _ = dx.loc(v)
        parent = v.semantic_parent
        if parent is not None and parent.kind == K.FUNCTION_DECL and v.storage_class == ci.StorageClass.STATIC:
            return self.g_by_key.get(f"{name}@{f}:{parent.spelling}")
        if v.storage_class == ci.StorageClass.STATIC:
            return self.g_by_key.get(f"{name}@{f}")
        cands = self.g_by_name.get(name, [])
        cands = [c for c in cands if c["linkage"] in ("extern", "per-instance")]
        if len(cands) == 1:
            return cands[0]["id"]
        ov = overlay_of(tu_rel)
        for c in cands:
            if c["overlay"] == ov or ov in c["instances"].split():
                return c["id"]
        for res in RESIDENT:
            for c in cands:
                if c["overlay"] == res:
                    return c["id"]
        return None

    def struct_tids(self, type_spelling: str) -> list[str]:
        """Ledger rows of a member's struct/union type, from its C spelling (arrays and pointers do not descend).
        Several when the type has more than one layout."""
        if "*" in type_spelling or "[" in type_spelling:
            return []
        m = re.search(r"^(?:const |volatile )*(struct|union|enum) \(unnamed (?:struct |union |enum )?at (.+?):(\d+):(\d+)\)",
                      type_spelling)
        if m:
            key = norm_key(f"{m[1]}:(unnamed at {m[2]}:{m[3]}:{m[4]})")
            return [v for k, v in self.t_by_key.items() if k == key or k.startswith(key + "#")]
        name = re.sub(r"^(const |volatile )*(struct |union )?", "", type_spelling).strip()
        one = self.t_by_name.get(name)
        return [one] if one else self.t_by_name_multi.get(name, [])

    def ledger_layout(self, tid: str) -> list[tuple[int, int, str, str]]:
        """(bit offset, bit width, field id, C type) of each ledger member, from the ledger's own layout."""
        if tid not in self._layouts:
            g = self.gen
            try:
                lay = g.layout(tid)
            except KeyError:
                self._layouts[tid] = []
                return []
            ms, fs = g.members[tid], lay["fields"]
            offs = [f["offset"] if f["offset"] is not None and f["offset"] >= 0 else 0 for f in fs]
            out = []
            for i, (m, f) in enumerate(zip(ms, fs)):
                nxt = next((o for o in offs[i + 1:] if o > offs[i]), lay["size"] * 8)
                w = int(m["bits"]) if m.get("bits") else max(nxt - offs[i], 0)
                out.append((offs[i], w, m["id"], f.get("type", "")))
            self._layouts[tid] = out
        return self._layouts[tid]

    # PS1 SDK structs keep their member names; members of game structs that fall inside one resolve to sdk:Type.member
    SDK_LAYOUTS = {
        "SVECTOR": [("vx", 0, 16), ("vy", 16, 16), ("vz", 32, 16), ("pad", 48, 16)],
        "VECTOR": [("vx", 0, 32), ("vy", 32, 32), ("vz", 64, 32), ("pad", 96, 32)],
        "CVECTOR": [("r", 0, 8), ("g", 8, 8), ("b", 16, 8), ("cd", 24, 8)],
        "RECT": [("x", 0, 16), ("y", 16, 16), ("w", 32, 16), ("h", 48, 16)],
    }

    def field_by_offset(self, tid: str, bit: int, width: int, depth: int = 0, loose: bool = False) -> str | None:
        """Ledger member (or member path "T.a>T.b") covering the pinned member at `bit`/`width`: used for structs whose
        ledger layout was merged with upstream (UPSTREAM_LAYOUTS.csv), where pinned member names do not exist.
        When no member covers it, a second (loose) pass returns the member starting at the same offset with a
        "~w<width>" marker: the pinned member is a wider untyped view (e.g. char[4] over a short plus its padding)."""
        lay = self.ledger_layout(tid)
        exact = [x for x in lay if x[0] == bit and x[1] == width]
        if exact:
            return exact[0][2]
        for off, w, fid, typ in lay:
            if off <= bit and (bit + width <= off + w or (loose and off <= bit < off + w)):
                sdk = self.SDK_LAYOUTS.get(typ.strip())
                if sdk:
                    hit = next((n for n, o, ww in sdk if o == bit - off and (ww == width or loose)), None)
                    if hit:
                        return f"{fid}>sdk:{typ.strip()}.{hit}"
                if depth < 4:
                    for sub in self.struct_tids(typ):
                        rest = self.field_by_offset(sub, bit - off, width, depth + 1, loose)
                        if rest:
                            return f"{fid}>{rest}"
                if off == bit:
                    return fid
        if depth == 0 and not loose:
            hit = self.field_by_offset(tid, bit, width, 0, True)
            return f"{hit}~w{width}" if hit else None
        return None

    def function_id(self, fn: ci.Cursor, defs: dict[str, ci.Cursor], tu_rel: str) -> str | None:
        d = fn.get_definition() or defs.get(fn.spelling)
        if d is not None:
            f, _ = dx.loc(d)
            hit = self.cat_by_file.get((f.removeprefix("src/"), fn.spelling))
            if hit:
                return hit
        decl_ov = overlay_of(dx.loc(fn)[0]).split("/")[-1]
        for ov in [decl_ov] + module_order(tu_rel):
            ids = self.cat_by_ov.get((ov, fn.spelling))
            if ids:
                # a template function bound several times in one plugin has one catalog row per binding
                return ids[0] if len({self.cat_final[i] for i in ids}) == 1 else None
        ids = self.cat_by_name.get(fn.spelling, [])
        if len(ids) == 1:
            return ids[0]
        if ids and all(self.cat_ov[i].startswith("PLG") or self.cat_ov[i] == "INITBTL.PRG" for i in ids):
            # effect plugins share one load address: the effect catalog says which plugin is loaded
            return f"plugin:{fn.spelling}"
        if fn.spelling in LINK_ALIASES:
            return self.function_id_by_name(LINK_ALIASES[fn.spelling], tu_rel)
        if not ids and fn.spelling in self.sdk:
            return f"sdk:{fn.spelling}"  # SDK function declared by a game file instead of the SDK header
        return self.placeholder_target(fn.spelling, tu_rel)

    def function_id_by_name(self, name: str, tu_rel: str) -> str | None:
        for ov in module_order(tu_rel):
            ids = self.cat_by_ov.get((ov, name))
            if ids:
                return ids[0] if len({self.cat_final[i] for i in ids}) == 1 else None
        return None

    def placeholder_target(self, name: str, tu_rel: str) -> str | None:
        """func_XXXXXXXX declared under a stale placeholder name: the symbol files name the address."""
        m = re.fullmatch(r"func_([0-9A-F]{8})", name)
        if not m:
            return None
        addr = int(m[1], 16)
        for ov in module_order(tu_rel):
            real = self.symbols.get((ov, addr))
            if real:
                if real in self.sdk:
                    return f"sdk:{real}"
                ids = self.cat_by_ov.get((ov, real)) or self.cat_by_name.get(real, [])
                if len(ids) == 1:
                    return ids[0]
        return None


class RefIndex:
    def __init__(self, ledgers: Ledgers, macros: dict) -> None:
        self.L = ledgers
        self.macros = macros
        self.occ: dict[str, dict[tuple[int, int], dict]] = collections.defaultdict(dict)
        self.unresolved: collections.Counter = collections.Counter()
        self.unresolved_err: collections.Counter = collections.Counter()
        self.mismatch = 0
        self.tagless = 0
        self.mismatch_examples: list = []
        self.text: dict[str, list[bytes]] = {}

    def line_text(self, rel: str, line: int) -> bytes:
        if rel not in self.text:
            p = dx.SRC / rel.removeprefix("src/") if rel.startswith("src/") else dx.DECOMP / rel
            try:
                self.text[rel] = p.read_bytes().splitlines()
            except OSError:
                self.text[rel] = []
        lines = self.text[rel]
        return lines[line - 1] if 0 < line <= len(lines) else b""

    def record(self, c: ci.Cursor, kind: str, target: str | None, name: str, tu_rel: str, err_tu: bool) -> None:
        f, line, col = spelling_location(c.location)
        if not f:
            return
        rel = dx.rel(f)
        if dx.origin(rel) != "game":
            return
        text = self.line_text(rel, line)
        n = len(name.encode())
        if text[col - 1:col - 1 + n] != name.encode():
            if c.kind in (K.STRUCT_DECL, K.UNION_DECL, K.ENUM_DECL):
                self.tagless += 1  # `typedef struct {..} X;`: the name is recorded at the typedef
                return
            spots = self.macro_body_spots(text, col, name)
            if spots:
                for mrel, mline, mcol in spots:
                    self.add(mrel, mline, mcol, n, kind + "@macro", target, name, tu_rel, err_tu)
                return
            self.mismatch += 1
            if len(self.mismatch_examples) < 50:
                self.mismatch_examples.append([rel, line, col, kind, name, text[col - 1:col + 30].decode("latin-1")])
            return
        macro = c.location.line != line or dx.rel(c.location.file.name if c.location.file else None) != rel
        self.add(rel, line, col, n, kind + ("@macro" if macro else ""), target, name, tu_rel, err_tu)

    def macro_body_spots(self, text: bytes, col: int, name: str) -> list[tuple[str, int, int]]:
        """libclang reports a symbol used inside a macro body defined in the same file at the macro call. Find the
        macro called at `col` and return every occurrence of `name` in its #define (continuation lines included).
        Effect plugins bind template macros (`#define VS_RING_FUNCTION func_800F98E0`) before including a template,
        sometimes several times with different bindings, so every definition of the macro in this translation unit
        is searched for `name`."""
        m = re.match(rb"[A-Za-z_]\w*", text[col - 1:])
        if not m:
            return []
        macro = m.group(0).decode()
        defs = list(self.tu_macros.get(macro, []))
        g = self.macros.get(macro)
        if g and (g["file"], g["line"]) not in defs:
            defs.append((g["file"], g["line"]))
        rx = re.compile(rb"(?<![\w])" + re.escape(name.encode()) + rb"(?![\w])")
        out = []
        for mrel, mline in defs:
            if not mrel.startswith(("src/", "include/")):
                continue
            ln = mline
            while True:
                t = self.line_text(mrel, ln)
                if not t and ln > mline:
                    break
                start = t.find(b"define") + 6 if ln == mline else 0
                for mm in rx.finditer(t, start):
                    if (mrel, ln, mm.start() + 1) not in out:
                        out.append((mrel, ln, mm.start() + 1))
                if not t.rstrip().endswith(b"\\"):
                    break
                ln += 1
        return out

    def add(self, rel, line, col, n, k, target, name, tu_rel, err_tu) -> None:
        kind = k.split("@")[0]
        if target is None:
            self.unresolved[(kind, name)] += 1
            if err_tu:
                self.unresolved_err[(kind, name, tu_rel)] += 1
            target = f"?{kind}:{name}"
        slot = self.occ[rel].setdefault((line, col), {"len": n, "kind": k, "targets": {}})
        slot["targets"].setdefault(target, set()).add(overlay_of(tu_rel))

    def parse(self, path: Path, idx: ci.Index) -> bool:
        tu_rel = dx.rel(str(path))
        tu = idx.parse(str(path), args=dx.ARGS, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        err = any(d.severity >= 3 for d in tu.diagnostics)
        self.tu_macros = collections.defaultdict(list)
        for c in tu.cursor.get_children():
            if c.kind == K.MACRO_DEFINITION and c.location.file:
                f, line = dx.loc(c)
                if dx.origin(f) in ("game", "gen"):
                    self.tu_macros[c.spelling].append((f, line))
        defs = {c.spelling: c for c in tu.cursor.get_children() if c.kind == K.FUNCTION_DECL and c.is_definition()}
        for c in tu.cursor.walk_preorder():
            k = c.kind
            loc_file = c.location.file
            if loc_file is None:
                continue
            if k in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR, K.TYPE_REF, K.MEMBER_REF):
                r = c.referenced
                if r is None:
                    continue
                target = self.resolve(r, defs, tu_rel)
                if target is not False:
                    self.record(c, *target, tu_rel, err)
            elif k in (K.FUNCTION_DECL, K.VAR_DECL, K.FIELD_DECL, K.ENUM_CONSTANT_DECL, K.STRUCT_DECL,
                       K.UNION_DECL, K.ENUM_DECL, K.TYPEDEF_DECL) and c.spelling:
                if k in (K.STRUCT_DECL, K.UNION_DECL, K.ENUM_DECL) and not c.spelling:
                    continue
                target = self.resolve(c, defs, tu_rel)
                if target is not False:
                    self.record(c, *target, tu_rel, err)
        return err

    def resolve(self, r: ci.Cursor, defs: dict, tu_rel: str):
        """(kind, target, spelling), or False when the declaration is not renamed (locals, SDK types...)."""
        L, rk = self.L, r.kind
        rf, _ = dx.loc(r)
        if rk == K.FUNCTION_DECL:
            d = r.get_definition() or defs.get(r.spelling)
            df = dx.loc(d)[0] if d is not None else rf
            if dx.origin(df) == "sdk" or (d is None and dx.origin(rf) == "sdk"):
                return ("func", f"sdk:{r.spelling}", r.spelling)
            return ("func", L.function_id(r, defs, tu_rel), r.spelling)
        if rk == K.VAR_DECL:
            if not dx.is_static_storage(r) or dx.origin(rf) == "sdk":
                return False
            return ("var", L.global_id(r, tu_rel), r.spelling)
        if rk in (K.STRUCT_DECL, K.UNION_DECL, K.ENUM_DECL, K.TYPEDEF_DECL):
            if dx.origin(rf) != "game" or not r.spelling or r.spelling.startswith(("struct (", "union (", "enum (")):
                return False
            tid = L.type_id(r)
            if tid is None and rk != K.TYPEDEF_DECL and r.get_definition() is None:
                return False  # forward declaration of a struct never defined anywhere: nothing to rename to
            return ("type", tid, r.spelling)
        if rk in (K.FIELD_DECL, K.ENUM_CONSTANT_DECL):
            parent = r.semantic_parent
            if parent is None or dx.origin(rf) != "game":
                return False
            tid = L.type_id(parent)
            fid = L.fields.get(tid, {}).get(r.spelling) if tid else None
            if fid is None and tid and rk == K.FIELD_DECL:
                bit = r.get_field_offsetof()
                width = r.get_bitfield_width() if r.is_bitfield() else r.type.get_size() * 8
                if bit is not None and bit >= 0 and width > 0:
                    fid = L.field_by_offset(tid, bit, width)
            return ("field" if rk == K.FIELD_DECL else "enum", fid, r.spelling)
        return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--out", type=Path, default=OUT)
    a = ap.parse_args()
    index = json.loads((REPO / "build" / "port" / "index.json").read_text(encoding="utf-8"))
    t0 = time.time()
    ri = RefIndex(Ledgers(index), index["macros"])
    idx = ci.Index.create()
    files = dx.game_tus(a.only)
    err_tus = [p for p in files if ri.parse(p, idx)]
    out_files, kinds, multi = {}, collections.Counter(), 0
    for rel, occ in sorted(ri.occ.items()):
        rows = []
        for (line, col), o in sorted(occ.items()):
            tg = o["targets"]
            if len(tg) == 1:
                target = next(iter(tg))
            else:
                multi += 1
                target = " ".join(f"{t}|{','.join(sorted(ovs))}" for t, ovs in sorted(tg.items()))
            rows.append([line, col, o["len"], o["kind"], target])
            kinds[o["kind"]] += 1
        out_files[rel] = rows
    total = sum(kinds.values())
    unresolved = sum(1 for rows in out_files.values() for r in rows if "?" in r[4])
    stats = {"translation_units": len(files), "error_tus": len(err_tus), "files": len(out_files),
             "occurrences": total, "by_kind": dict(kinds.most_common()), "multi_target": multi,
             "unresolved_occurrences": unresolved, "text_mismatch_skipped": ri.mismatch,
             "tagless_typedef_structs": ri.tagless,
             "unresolved_in_error_tus": sum(ri.unresolved_err.values()), "seconds": round(time.time() - t0)}
    top = [{"kind": k, "name": n, "count": c} for (k, n), c in ri.unresolved.most_common(200)]
    err = [{"kind": k, "name": n, "tu": t, "count": c} for (k, n, t), c in ri.unresolved_err.most_common()]
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({"stats": stats, "unresolved": top, "unresolved_in_error_tus": err,
                                 "mismatch_examples": ri.mismatch_examples, "files": out_files},
                                separators=(",", ":")), encoding="utf-8")
    print(json.dumps(stats, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
