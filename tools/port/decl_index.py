"""Declaration extents: where every top-level declaration in game code is written, mapped to its ledger row.

The copy tool moves text (function bodies, prototypes, global definitions, type definitions, macros) out of the
decomp by byte range, then renames inside each range from build/port/refs.json. This pass records those ranges with
the same libclang setup and the same resolver as ref_index.py, so the ids agree.

Output build/port/decls.json (git-ignored):
  {"decls": [{"file", "start", "end", "line", "kind", "name", "def", "targets": [...], "tus": [...]}, ...],
   "macros": {"src/...h:LINE": {"name", "file", "start", "end", "line"}},
   "tu_macros": {"src/...c": {"NAME": "src/...h:LINE"}}}
`start`/`end` are byte offsets of the libclang extent (end exclusive). `kind` is function/var/type/macro. A header
declaration seen from several translation units lists every target it resolved to.

  python tools\\port\\decl_index.py [--only <substring>]
"""
from __future__ import annotations

import argparse
import collections
import json
import sys
import time
from pathlib import Path

import clang.cindex as ci

sys.path.insert(0, str(Path(__file__).resolve().parent))
import decomp_index as dx  # noqa: E402
import ref_index as rx  # noqa: E402

K = ci.CursorKind
OUT = dx.REPO / "build" / "port" / "decls.json"
KINDS = {K.FUNCTION_DECL: "function", K.VAR_DECL: "var", K.STRUCT_DECL: "type", K.UNION_DECL: "type",
         K.ENUM_DECL: "type", K.TYPEDEF_DECL: "type"}


def type_needs(c: ci.Cursor, L: rx.Ledgers) -> tuple[set[str], set[str]]:
    """Ledger types this declaration needs complete (by value) and ones it only points to."""
    need, ptr = set(), set()
    types = []
    if c.kind == K.FUNCTION_DECL:
        types = [c.result_type] + [a.type for a in c.get_arguments()]
    elif c.kind == K.VAR_DECL:
        types = [c.type]
    elif c.kind in (K.STRUCT_DECL, K.UNION_DECL):
        stack = [c]
        while stack:
            s = stack.pop()
            for f in s.get_children():
                if f.kind == K.FIELD_DECL:
                    types.append(f.type)
                elif f.kind in (K.STRUCT_DECL, K.UNION_DECL):
                    stack.append(f)  # nested definitions are written inline
    elif c.kind == K.TYPEDEF_DECL:
        u = c.underlying_typedef_type
        d = u.get_declaration()
        if d is not None and d.kind in (K.STRUCT_DECL, K.UNION_DECL) and d.is_definition() \
                and d.extent.start.offset >= c.extent.start.offset and d.extent.end.offset <= c.extent.end.offset:
            return type_needs(d, L)
        types = [u]
    for t in types:
        for sub in [t] + ([t.get_pointee().get_result()] if t.kind == ci.TypeKind.POINTER and
                          t.get_pointee().kind == ci.TypeKind.FUNCTIONPROTO else []):
            d, by_value = dx.decl_of_type(sub)
            if d is None or dx.origin(dx.loc(d)[0]) != "game":
                continue
            tid = L.type_id(d)
            if tid:
                (need if by_value else ptr).add(tid)
            if sub.kind == ci.TypeKind.POINTER and sub.get_pointee().kind == ci.TypeKind.FUNCTIONPROTO:
                for a in sub.get_pointee().argument_types():
                    d2, bv2 = dx.decl_of_type(a)
                    if d2 is not None and dx.origin(dx.loc(d2)[0]) == "game" and L.type_id(d2):
                        (need if bv2 else ptr).add(L.type_id(d2))
    return need, ptr - need


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    a = ap.parse_args()
    index = json.loads((dx.REPO / "build" / "port" / "index.json").read_text(encoding="utf-8"))
    t0 = time.time()
    ri = rx.RefIndex(rx.Ledgers(index), index["macros"])
    idx = ci.Index.create()
    decls: dict[tuple[str, int], dict] = {}
    macros: dict[str, dict] = {}
    tu_macros: dict[str, dict[str, str]] = {}
    for path in dx.game_tus(a.only):
        tu_rel = dx.rel(str(path))
        tu = idx.parse(str(path), args=dx.ARGS, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        defs = {c.spelling: c for c in tu.cursor.get_children() if c.kind == K.FUNCTION_DECL and c.is_definition()}
        own: dict[str, str] = {}
        for c in tu.cursor.get_children():
            if c.location.file is None:
                continue
            f = dx.rel(c.location.file.name)
            if dx.origin(f) not in ("game", "gen"):
                continue
            ext = c.extent
            if c.kind == K.MACRO_DEFINITION:
                key = f"{f}:{c.location.line}"
                macros.setdefault(key, {"name": c.spelling, "file": f, "line": c.location.line,
                                        "start": ext.start.offset, "end": ext.end.offset})
                own[c.spelling] = key
                continue
            kind = KINDS.get(c.kind)
            if kind is None or not c.spelling:
                continue
            res = ri.resolve(c, defs, tu_rel)
            target = None if res is False else (res[1] or f"?{res[0]}:{res[2]}")
            if target is None and kind == "type" and c.kind != K.TYPEDEF_DECL and c.is_definition():
                target = ri.L.type_id(c)  # anonymous struct declared with a variable: `struct {...} x[] = ...`
            d = decls.setdefault((f, ext.start.offset, c.kind.name, c.spelling), {
                "file": f, "start": ext.start.offset, "end": ext.end.offset, "line": c.location.line,
                "kind": kind, "ckind": c.kind.name, "name": c.spelling,
                "def": bool(c.is_definition()) if kind != "var" else c.storage_class != ci.StorageClass.EXTERN,
                "static": c.storage_class == ci.StorageClass.STATIC, "targets": [], "tus": []})
            d["end"] = max(d["end"], ext.end.offset)
            if target and target not in d["targets"]:
                d["targets"].append(target)
            if tu_rel not in d["tus"]:
                d["tus"].append(tu_rel)
            if "need" not in d:
                need, ptr = type_needs(c, ri.L)
                d["need"], d["ptr"] = sorted(need), sorted(ptr)
                d["ctype"] = c.type.get_canonical().spelling
                if c.kind == K.FUNCTION_DECL:
                    rt = c.result_type
                    d["ret"] = rt.spelling
                    d["ret_kind"] = rt.get_canonical().kind.name
                    d["nparams"] = len(list(c.get_arguments()))
                    d["variadic"] = c.type.kind == ci.TypeKind.FUNCTIONPROTO and c.type.is_function_variadic()
                elif c.kind == K.VAR_DECL:
                    t = c.type
                    d["type"] = t.spelling
                    if t.kind == ci.TypeKind.INCOMPLETEARRAY:
                        d["elem_size"] = t.element_type.get_size()
        tu_macros[tu_rel] = own
    out = sorted(decls.values(), key=lambda d: (d["file"], d["start"]))
    OUT.write_text(json.dumps({"decls": out, "macros": macros, "tu_macros": tu_macros}, separators=(",", ":")),
                   encoding="utf-8")
    c = collections.Counter((d["kind"], d["def"]) for d in out)
    print(f"{len(out)} declarations {dict(c)}, {len(macros)} macro definitions, "
          f"{sum(1 for d in out if not d['targets'])} unmapped, {round(time.time() - t0)} s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
