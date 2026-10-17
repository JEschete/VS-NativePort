"""Index the decomp with libclang: functions, globals, types, macros and per-function references.

Output: build/port/index.json (git-ignored), consumed by copy_to_native.py and the type/data ledger builders.

Every game translation unit (non-SDK .c under resources/rood-reverseWin/src) is parsed with the decomp's own
preprocessor flags plus build/port/gen (see gen_decomp_headers.py). Declarations are de-duplicated across
translation units by (kind, name, file, line).

Usage: python tools/port/decomp_index.py [--only <substring>] [--root <decomp checkout>] [--out <json>]

--root indexes another checkout with the same layout (e.g. resources/rood-reverse, the upstream main branch) so
tools/naming/upstream_layout.py can compare struct layouts against the pinned reference.
"""
from __future__ import annotations

import argparse
import collections
import json
import sys
import time
from pathlib import Path

import clang.cindex as ci

REPO = Path(__file__).resolve().parents[2]
DECOMP = REPO / "resources" / "rood-reverseWin"
SRC = DECOMP / "src"
PSXINC = DECOMP / "include" / "psx"
GEN = REPO / "build" / "port" / "gen"
OUT = REPO / "build" / "port" / "index.json"

sys.path.insert(0, str(REPO / "tools" / "ledger"))
from build_ledgers import is_sdk_path  # noqa: E402

def clang_args() -> list[str]:
    return [
        "-target", "mipsel-unknown-none", "-nostdinc",
        "-I", str(PSXINC), "-I", str(DECOMP / "src" / "include"), "-I", str(DECOMP), "-I", str(GEN),
        "-D__attribute__(x)=", "-D_LANGUAGE_C", "-DMENUBGSZ=1", "-std=gnu89", "-fms-extensions", "-Wno-everything",
    ]


ARGS = clang_args()


def configure(root: Path) -> None:
    global DECOMP, SRC, PSXINC, ARGS
    DECOMP = root.resolve()
    SRC = DECOMP / "src"
    PSXINC = DECOMP / "include" / "psx"
    ARGS = clang_args()
K = ci.CursorKind
TYPE_DECLS = {K.STRUCT_DECL: "struct", K.UNION_DECL: "union", K.ENUM_DECL: "enum", K.TYPEDEF_DECL: "typedef"}


def rel(path: str | None) -> str:
    if not path:
        return ""
    p = Path(path).resolve()
    for root, tag in ((SRC, "src/"), (PSXINC, "psx/"), (DECOMP / "include", "include/"), (GEN, "gen/"), (DECOMP, "")):
        try:
            return tag + p.relative_to(root).as_posix()
        except ValueError:
            pass
    return p.as_posix()


def origin(r: str) -> str:
    """game | sdk | gen | other"""
    if r.startswith("psx/"):
        return "sdk"
    if r.startswith("gen/"):
        return "gen"
    if r.startswith("src/"):
        return "sdk" if is_sdk_path(Path(r[4:])) else "game"
    if r.startswith("include/"):
        return "game"
    return "other"


def loc(c: ci.Cursor) -> tuple[str, int]:
    f = c.location.file
    return rel(f.name if f else None), c.location.line


def decl_of_type(t: ci.Type) -> tuple[ci.Cursor | None, bool]:
    """Underlying declaration of a field/var type, and whether the dependency is by-value (needs full definition)."""
    by_value = True
    while True:
        k = t.kind
        if k in (ci.TypeKind.POINTER, ci.TypeKind.BLOCKPOINTER):
            by_value = False
            t = t.get_pointee()
        elif k in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY, ci.TypeKind.VARIABLEARRAY):
            t = t.element_type
        elif k == ci.TypeKind.ELABORATED:
            t = t.get_named_type()
        elif k == ci.TypeKind.FUNCTIONPROTO:
            return None, False
        else:
            break
    d = t.get_declaration()
    if d is None or d.kind == K.NO_DECL_FOUND:
        return None, by_value
    return d, by_value


def is_static_storage(v: ci.Cursor) -> bool:
    """File-scope variable, block-scope `extern`, or function-local `static`."""
    if v.semantic_parent is not None and v.semantic_parent.kind == K.TRANSLATION_UNIT:
        return True
    return v.storage_class in (ci.StorageClass.EXTERN, ci.StorageClass.STATIC)


def type_key(d: ci.Cursor) -> str:
    kind = TYPE_DECLS.get(d.kind, d.kind.name)
    name = d.spelling or f"<anon@{loc(d)[0]}:{loc(d)[1]}>"
    if kind in ("struct", "union", "enum") and name.startswith(("struct ", "union ", "enum ")):
        name = name.split(" ", 1)[1]
    return f"{kind}:{name}"


class Index:
    def __init__(self) -> None:
        self.functions: dict[str, dict] = {}
        self.globals: dict[str, dict] = {}
        self.types: dict[str, dict] = {}
        self.macros: dict[str, dict] = {}
        self.enum_consts: dict[str, dict] = {}
        self.sdk_types = collections.Counter()
        self.tu_errors: dict[str, list[str]] = {}
        self.cur_tu = ""

    # ------------------------------------------------------------------ declarations
    def add_type(self, c: ci.Cursor) -> None:
        if not c.is_definition() and c.kind != K.TYPEDEF_DECL:
            return
        f, line = loc(c)
        if origin(f) != "game":
            return
        key = type_key(c)
        uid = f"{key}@{f}:{line}"
        if uid in self.types:
            return
        deps: dict[str, bool] = {}
        fields = []
        if c.kind == K.TYPEDEF_DECL:
            d, by_value = decl_of_type(c.underlying_typedef_type)
            if d is not None and d.kind in TYPE_DECLS:
                deps[type_key(d)] = deps.get(type_key(d), False) or by_value
        elif c.kind in (K.STRUCT_DECL, K.UNION_DECL):
            for fc in c.get_children():
                if fc.kind in TYPE_DECLS and fc.kind != K.TYPEDEF_DECL:
                    self.add_type(fc)  # nested definitions at any depth (struct in union in struct)
                if fc.kind == K.FIELD_DECL:
                    fields.append({"name": fc.spelling, "type": fc.type.spelling,
                                   "bits": fc.get_bitfield_width() if fc.is_bitfield() else 0,
                                   "offset": fc.get_field_offsetof()})
                    d, by_value = decl_of_type(fc.type)
                    if d is not None and d.kind in TYPE_DECLS:
                        deps[type_key(d)] = deps.get(type_key(d), False) or by_value
        elif c.kind == K.ENUM_DECL:
            for ec in c.get_children():
                if ec.kind == K.ENUM_CONSTANT_DECL:
                    fields.append({"name": ec.spelling, "value": ec.enum_value})
                    self.enum_consts.setdefault(ec.spelling, {"file": f, "line": ec.location.line, "enum": key})
        e = c.extent
        self.types[uid] = {"key": key, "file": f, "line": line, "end_line": e.end.line, "anon": not c.spelling,
                           "fields": fields, "deps": deps, "size": c.type.get_size() if c.kind != K.ENUM_DECL else 4}

    def add_var(self, c: ci.Cursor, scope: str | None = None) -> None:
        """File-scope variables, plus (with `scope` = the enclosing function) block-scope `extern` declarations and
        function-local statics: both name static storage that the port must define."""
        if scope is None and (c.semantic_parent is None or c.semantic_parent.kind != K.TRANSLATION_UNIT):
            return
        f, line = loc(c)
        if origin(f) != "game":
            return
        g = self.globals.setdefault(c.spelling, {"name": c.spelling, "decls": [], "defs": []})
        # A file-scope declaration without `extern` is a definition; without an initializer it is a tentative
        # (zero-initialized) definition, which libclang does not report through is_definition().
        is_def = c.storage_class != ci.StorageClass.EXTERN or c.is_definition()
        has_init = any(ch.kind.is_expression() or ch.kind == K.INIT_LIST_EXPR for ch in c.get_children())
        rec = {"file": f, "line": line, "end_line": c.extent.end.line, "type": c.type.spelling,
               "static": c.storage_class == ci.StorageClass.STATIC, "size": c.type.get_size()}
        if scope is not None:
            rec["scope"] = scope
        d, _ = decl_of_type(c.type)
        if d is not None and d.kind in TYPE_DECLS:
            rec["type_key"] = type_key(d)
        target = g["defs"] if is_def else g["decls"]
        if is_def:
            rec["init"] = has_init
        prev = next((x for x in target if x["file"] == f and x["line"] == line), None)
        if prev is None:
            rec["tus"] = [self.cur_tu]
            target.append(rec)
        elif self.cur_tu not in prev["tus"]:
            prev["tus"].append(self.cur_tu)

    def add_function(self, c: ci.Cursor, src_bytes: bytes, tu_file: str) -> None:
        f, line = loc(c)
        if origin(f) != "game":
            return
        uid = f"{c.spelling}@{f}:{line}"
        if uid in self.functions:
            return
        e = c.extent
        refs: dict[str, collections.Counter] = {k: collections.Counter() for k in ("func", "var", "type", "enum", "field")}
        for n in c.walk_preorder():
            k = n.kind
            if k in (K.DECL_REF_EXPR, K.CALL_EXPR, K.TYPE_REF, K.MEMBER_REF_EXPR):
                r = n.referenced
                if r is None:
                    continue
                rk = r.kind
                if rk == K.FUNCTION_DECL:
                    refs["func"][r.spelling] += k == K.DECL_REF_EXPR
                elif rk == K.VAR_DECL and is_static_storage(r):
                    refs["var"][r.spelling] += 1
                elif rk == K.ENUM_CONSTANT_DECL:
                    refs["enum"][r.spelling] += 1
                elif rk in TYPE_DECLS:
                    rf, _ = loc(r)
                    if origin(rf) == "sdk":
                        self.sdk_types[r.spelling] += 1
                    refs["type"][type_key(r)] += 1
                elif rk == K.FIELD_DECL:
                    refs["field"][f"{r.semantic_parent.spelling}.{r.spelling}"] += 1
            elif k in (K.VAR_DECL, K.PARM_DECL, K.CSTYLE_CAST_EXPR, K.UNEXPOSED_EXPR) and n is not c:
                if k == K.VAR_DECL and is_static_storage(n):
                    self.add_var(n, scope=c.spelling)
                d, _ = decl_of_type(n.type)
                if d is not None and d.kind in TYPE_DECLS:
                    rf, _ = loc(d)
                    if origin(rf) == "sdk":
                        self.sdk_types[d.spelling] += 1
                    refs["type"][type_key(d)] += 1
            elif k in TYPE_DECLS:
                self.add_type(n)
        ret_d, _ = decl_of_type(c.result_type)
        if ret_d is not None and ret_d.kind in TYPE_DECLS:
            refs["type"][type_key(ret_d)] += 1
        self.functions[uid] = {
            "name": c.spelling, "file": f, "line": line, "end_line": e.end.line,
            "start": e.start.offset if e.start.file and rel(e.start.file.name) == f else None,
            "end": e.end.offset if e.end.file and rel(e.end.file.name) == f else None,
            "static": c.storage_class == ci.StorageClass.STATIC,
            "signature": c.type.spelling, "result": c.result_type.spelling,
            "params": [{"name": a.spelling, "type": a.type.spelling} for a in c.get_arguments()],
            "refs": {k: dict(v) for k, v in refs.items() if v},
        }

    def add_macro_def(self, c: ci.Cursor) -> None:
        f, line = loc(c)
        if origin(f) not in ("game", "gen"):
            return
        self.macros.setdefault(c.spelling, {"file": f, "line": line, "uses": 0})

    # ------------------------------------------------------------------ driver
    def parse(self, path: Path, idx: ci.Index) -> None:
        self.cur_tu = rel(str(path))
        tu = idx.parse(str(path), args=ARGS, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        errs = [f"{d.location.line}: {d.spelling}" for d in tu.diagnostics if d.severity >= 3]
        if errs:
            self.tu_errors[rel(str(path))] = errs[:10]
        src_bytes = path.read_bytes()
        macro_uses = collections.Counter()
        for c in tu.cursor.get_children():
            k = c.kind
            if k == K.MACRO_DEFINITION:
                self.add_macro_def(c)
            elif k == K.MACRO_INSTANTIATION:
                macro_uses[c.spelling] += 1
            elif k == K.FUNCTION_DECL and c.is_definition():
                self.add_function(c, src_bytes, rel(str(path)))
            elif k == K.VAR_DECL:
                self.add_var(c)
            elif k in TYPE_DECLS:
                self.add_type(c)
                for ch in c.get_children():
                    if ch.kind in TYPE_DECLS:
                        self.add_type(ch)
            if k == K.TYPEDEF_DECL:
                for ch in c.get_children():
                    if ch.kind in (K.STRUCT_DECL, K.UNION_DECL, K.ENUM_DECL):
                        self.add_type(ch)
        for name, n in macro_uses.items():
            if name in self.macros:
                self.macros[name]["uses"] += n

    def macros_per_function(self, files: list[Path], idx: ci.Index) -> None:
        """Second pass: attribute macro instantiations to functions by source range."""
        by_file = collections.defaultdict(list)
        for uid, fn in self.functions.items():
            by_file[fn["file"]].append(fn)
        for path in files:
            r = rel(str(path))
            fns = sorted(by_file.get(r, []), key=lambda x: x["line"])
            if not fns:
                continue
            tu = idx.parse(str(path), args=ARGS, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
            for c in tu.cursor.get_children():
                if c.kind != K.MACRO_INSTANTIATION or not c.location.file or rel(c.location.file.name) != r:
                    continue
                line = c.location.line
                for fn in fns:
                    if fn["line"] <= line <= fn["end_line"]:
                        fn["refs"].setdefault("macro", {})
                        fn["refs"]["macro"][c.spelling] = fn["refs"]["macro"].get(c.spelling, 0) + 1
                        break


def game_tus(only: str | None) -> list[Path]:
    out = []
    for p in sorted(SRC.rglob("*.c")):
        r = p.relative_to(SRC)
        if is_sdk_path(r):
            continue
        if only and only not in r.as_posix():
            continue
        out.append(p)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--root", type=Path)
    ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    if a.root:
        configure(a.root)
    out = a.out or OUT
    if not (GEN / "build" / "src" / "include" / "lbas.h").exists():
        sys.exit("run tools/port/gen_decomp_headers.py first")
    t0 = time.time()
    idx = ci.Index.create()
    ix = Index()
    files = game_tus(a.only)
    for p in files:
        ix.parse(p, idx)
    ix.macros_per_function(files, idx)
    out.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "functions": ix.functions, "globals": ix.globals, "types": ix.types, "macros": ix.macros,
        "enum_consts": ix.enum_consts, "sdk_types": dict(ix.sdk_types.most_common()), "tu_errors": ix.tu_errors,
    }
    out.write_text(json.dumps(data, indent=1), encoding="utf-8")
    print(f"{len(files)} TUs in {time.time() - t0:.0f}s: {len(ix.functions)} functions, {len(ix.globals)} globals, "
          f"{len(ix.types)} types, {len(ix.macros)} macros, {len(ix.enum_consts)} enum consts, "
          f"{len(ix.sdk_types)} SDK types used, {len(ix.tu_errors)} TUs with errors -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
