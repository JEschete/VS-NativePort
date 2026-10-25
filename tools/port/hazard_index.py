"""x64 hazard index: where the decomp turns pointers into 32-bit integers, and where it writes struct sizes as
literals (PORTING_WATCHLIST.md §2). The copy tool applies the resulting edits by position, like refs.json renames.

  ptr_casts   every `(int)ptr`-style cast to a 4-byte integer, with where its value goes:
                link            `(T)p & 0xFFFFFF`: an ordering-table link -> LegacyGpu_Handle(p) & 0xFFFFFF
                field:<id>      stored into a struct member     -> the member becomes uintptr_t
                global:<id>     stored into a global            -> the global becomes uintptr_t
                local           stored into a local variable    -> the local becomes uintptr_t (one declarator)
                param:<F>:<n>, return:<F>, compare, expr, deref ...  reported (signature work for modernization)
              The cast itself becomes (uintptr_t), so the value is never truncated before it lands.
  decls       the type span of each member/global/local declaration named above, so the copy tool can retype it.
  size_literals  integer literals passed with a pointer to (or assigned into a pointer to) a struct that contains
              pointers, equal to that struct's 32-bit size (or to the size of an array of it) -> sizeof.

Output build/port/hazards.json.  python tools\\port\\hazard_index.py [--only <substring>]
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
import time
from pathlib import Path

import clang.cindex as ci

sys.path.insert(0, str(Path(__file__).resolve().parent))
import decomp_index as dx  # noqa: E402
import ref_index as rx  # noqa: E402

K = ci.CursorKind
T = ci.TypeKind
OUT = dx.REPO / "build" / "port" / "hazards.json"
INT_KINDS = {T.INT, T.UINT, T.LONG, T.ULONG}
PTR_KINDS = {T.POINTER, T.CONSTANTARRAY, T.INCOMPLETEARRAY, T.FUNCTIONPROTO, T.FUNCTIONNOPROTO}
BASE_INT = re.compile(r"\b(unsigned\s+long|unsigned\s+int|signed\s+int|unsigned|long|int|u_long|u_int|uint|ulong)\b")


class Hazards:
    def __init__(self, ri: rx.RefIndex) -> None:
        self.ri = ri
        self.casts: dict[tuple, dict] = {}
        self.decls: dict[str, dict] = {}       # dest key -> {file, start, end, text, multi}
        self.sizes: dict[tuple, dict] = {}
        self.skipped = collections.Counter()
        self._ptr_struct: dict[str, bool] = {}

    # ------------------------------------------------------------------ helpers
    @staticmethod
    def spell(c: ci.Cursor) -> tuple[str | None, int]:
        f, line, col = rx.spelling_location(c.extent.start)
        return (dx.rel(f) if f else None), c.extent.start.offset

    @staticmethod
    def in_macro(c: ci.Cursor) -> bool:
        f, line, col = rx.spelling_location(c.extent.start)
        loc = c.extent.start
        return f is None or loc.file is None or dx.rel(f) != dx.rel(loc.file.name) or line != loc.line

    def contains_pointer(self, t: ci.Type, depth: int = 0) -> bool:
        t = t.get_canonical()
        if t.kind in (T.POINTER, T.FUNCTIONPROTO):
            return True
        if t.kind in (T.CONSTANTARRAY, T.INCOMPLETEARRAY):
            return self.contains_pointer(t.element_type, depth + 1)
        if t.kind != T.RECORD or depth > 8:
            return False
        key = t.spelling
        if key not in self._ptr_struct:
            self._ptr_struct[key] = False
            self._ptr_struct[key] = any(self.contains_pointer(f.type, depth + 1) for f in t.get_fields())
        return self._ptr_struct[key]

    def decl_span(self, d: ci.Cursor) -> dict | None:
        """The base integer type span of a member/variable declaration (for retyping), from its spelling."""
        f, start = self.spell(d)
        if not f or dx.origin(f) != "game" or self.in_macro(d):
            return None
        name_off = d.location.offset
        src = self.ri.line_text  # noqa: F841 (keeps RefIndex text cache warm)
        data = (dx.SRC / f[4:]).read_bytes() if f.startswith("src/") else None
        if data is None:
            return None
        head = data[start:name_off].decode("latin-1")
        if "*" in head or "(" in head:
            return None
        m = BASE_INT.search(head)
        if not m:
            return None
        tail_end = d.extent.end.offset
        rest = data[name_off:tail_end + 2].decode("latin-1")
        multi = "," in rest.split(";")[0] and "{" not in rest.split(";")[0]
        bitfield = d.kind == K.FIELD_DECL and d.is_bitfield()
        if bitfield:
            return None
        return {"file": f, "start": start + m.start(), "end": start + m.end(), "text": m[0], "multi": multi}

    def operator(self, c: ci.Cursor, lhs: ci.Cursor) -> str:
        end = lhs.extent.end.offset
        for t in c.get_tokens():
            if t.extent.start.offset >= end and t.kind == ci.TokenKind.PUNCTUATION:
                return t.spelling
        return ""

    # ------------------------------------------------------------------ pointer casts
    def destination(self, stack: list[ci.Cursor], i: int) -> tuple[str, ci.Cursor | None]:
        """Walk up from the cast (stack[i]) to where its value goes."""
        child = stack[i]
        for j in range(i - 1, -1, -1):
            p = stack[j]
            k = p.kind
            if k in (K.PAREN_EXPR, K.UNEXPOSED_EXPR):
                child = p
                continue
            if k == K.BINARY_OPERATOR:
                kids = list(p.get_children())
                if len(kids) != 2:
                    return "expr", None
                op = self.operator(p, kids[0])
                if op in ("&",) and any(re.fullmatch(r"0x0*[Ff]{6}[uU]?", t.spelling) for t in kids[1 if kids[0] == child else 0].get_tokens()):
                    return "link", None
                if op == "=" and kids[1] == child:
                    return self.lvalue(kids[0])
                if op in ("+", "-", "|", "^", "<<", ">>", "&"):
                    child = p
                    continue
                if op in ("==", "!=", "<", ">", "<=", ">="):
                    return "compare", None
                return "expr", None
            if k == K.COMPOUND_ASSIGNMENT_OPERATOR:
                kids = list(p.get_children())
                return self.lvalue(kids[0]) if kids and kids[-1] == child else ("expr", None)
            if k == K.VAR_DECL:
                return self.variable(p)
            if k == K.CALL_EXPR:
                args = list(p.get_arguments())
                fn = p.referenced
                n = next((a for a, x in enumerate(args) if x == child or
                          (x.extent.start.offset <= child.extent.start.offset <= x.extent.end.offset)), -1)
                target = self.ri.resolve(fn, {}, self.tu) if fn is not None else False
                name = target[1] if target and target is not False and target[1] else (fn.spelling if fn else "?")
                return f"param:{name}:{n}", None
            if k == K.RETURN_STMT:
                fn = next((stack[x] for x in range(j, -1, -1) if stack[x].kind == K.FUNCTION_DECL), None)
                target = self.ri.resolve(fn, {}, self.tu) if fn is not None else False
                return f"return:{target[1] if target else '?'}", None
            if k == K.CSTYLE_CAST_EXPR:
                return "recast", None
            if k == K.INIT_LIST_EXPR:
                return "initializer", None
            return k.name.lower(), None
        return "expr", None

    def lvalue(self, lhs: ci.Cursor) -> tuple[str, ci.Cursor | None]:
        c = lhs
        while c.kind in (K.PAREN_EXPR, K.UNEXPOSED_EXPR, K.ARRAY_SUBSCRIPT_EXPR):
            kids = list(c.get_children())
            if not kids:
                break
            c = kids[0]
        if c.kind == K.MEMBER_REF_EXPR and c.referenced is not None:
            res = self.ri.resolve(c.referenced, {}, self.tu)
            if res and res[1]:
                return f"field:{res[1]}", c.referenced
            return "field:?", None
        if c.kind == K.DECL_REF_EXPR and c.referenced is not None and c.referenced.kind == K.VAR_DECL:
            return self.variable(c.referenced)
        return "deref", None

    def variable(self, v: ci.Cursor) -> tuple[str, ci.Cursor | None]:
        if dx.is_static_storage(v):
            res = self.ri.resolve(v, {}, self.tu)
            return (f"global:{res[1]}", v) if res and res[1] else ("global:?", None)
        if v.kind == K.PARM_DECL:
            return "param-local", None
        f, off = self.spell(v)
        return f"local:{f}:{off}", v

    def cast(self, stack: list[ci.Cursor]) -> None:
        c = stack[-1]
        kids = list(c.get_children())
        if not kids:
            return
        operand = kids[-1]
        if c.type.get_canonical().kind not in INT_KINDS or operand.type.get_canonical().kind not in PTR_KINDS:
            return
        f, start = self.spell(c)
        if not f or dx.origin(f) != "game":
            return
        if self.in_macro(c):
            self.skipped["cast-in-macro"] += 1
            return
        toks = list(c.get_tokens())
        if not toks or toks[0].spelling != "(":
            return
        depth, close = 0, None
        for t in toks:
            depth += {"(": 1, ")": -1}.get(t.spelling, 0)
            if depth == 0:
                close = t
                break
        if close is None:
            return
        dest, decl = self.destination(stack, len(stack) - 1)
        key = (f, start)
        if key in self.casts:
            return
        rec = {"file": f, "start": start, "close": close.extent.end.offset,
               "operand_end": operand.extent.end.offset, "dest": dest,
               "line": c.extent.start.line, "text": "".join(t.spelling for t in toks[:6])}
        self.casts[key] = rec
        if decl is not None and dest not in self.decls:
            span = self.decl_span(decl)
            if span:
                self.decls[dest] = span
            else:
                self.skipped["dest-decl-not-retypable"] += 1
        if decl is not None and dest.startswith(("field:", "global:")):
            self.extra_decls(dest, decl)

    def extra_decls(self, dest: str, decl: ci.Cursor) -> None:
        """Other declarations of the same global (externs in other files) are retyped too."""
        key = dest + "#" + "{}:{}".format(*self.spell(decl))
        if key not in self.decls:
            span = self.decl_span(decl)
            if span:
                self.decls[key] = span

    # ------------------------------------------------------------------ size literals
    def literal(self, c: ci.Cursor) -> tuple[int, ci.Cursor] | None:
        while c.kind in (K.PAREN_EXPR, K.UNEXPOSED_EXPR, K.CSTYLE_CAST_EXPR):
            kids = list(c.get_children())
            if not kids:
                return None
            c = kids[-1]
        if c.kind != K.INTEGER_LITERAL:
            return None
        toks = list(c.get_tokens())
        if not toks:
            return None
        try:
            return int(re.sub(r"[uUlL]+$", "", toks[0].spelling), 0), c
        except ValueError:
            return None

    def pointee(self, t: ci.Type) -> tuple[ci.Type, int] | None:
        t = t.get_canonical()
        if t.kind == T.POINTER:
            p = t.get_pointee().get_canonical()
            return (p, 1) if p.kind == T.RECORD else None
        if t.kind == T.CONSTANTARRAY and t.element_type.get_canonical().kind == T.RECORD:
            return t.element_type.get_canonical(), t.element_count
        return None

    def call(self, stack: list[ci.Cursor]) -> None:
        c = stack[-1]
        args = list(c.get_arguments())
        lits = [(i, self.literal(a)) for i, a in enumerate(args)]
        lits = [(i, x) for i, x in lits if x]
        if not lits:
            return
        targets = []
        for a in args:
            pt = self.pointee(a.type)
            if pt is None:
                kids = list(a.get_children())
                pt = self.pointee(kids[-1].type) if a.kind == K.UNEXPOSED_EXPR and kids else None
            if pt:
                targets.append(pt)
        parent = stack[-2] if len(stack) > 1 else None
        for p in [x for x in reversed(stack[:-1]) if x.kind not in (K.PAREN_EXPR, K.UNEXPOSED_EXPR,
                                                                     K.CSTYLE_CAST_EXPR)][:1]:
            if p.kind == K.VAR_DECL:
                pt = self.pointee(p.type)
                if pt:
                    targets.append(pt)
            elif p.kind == K.BINARY_OPERATOR:
                kids = list(p.get_children())
                if len(kids) == 2:
                    pt = self.pointee(kids[0].type)
                    if pt:
                        targets.append(pt)
        del parent
        for _index, (value, lit) in lits:
            for rec, count in targets:
                size = rec.get_size()
                if size <= 0 or not self.contains_pointer(rec):
                    continue
                n = value // size if value % size == 0 else 0
                if n == count or (n == 1):
                    f, start = self.spell(lit)
                    if not f or dx.origin(f) != "game" or self.in_macro(lit):
                        continue
                    decl = rec.get_declaration()
                    res = self.ri.L.type_id(decl) if decl is not None else None
                    if not res:
                        self.skipped["size-type-unmapped"] += 1
                        continue
                    self.sizes[(f, start)] = {"file": f, "start": start, "end": lit.extent.end.offset,
                                              "value": value, "type": res, "count": n,
                                              "line": lit.extent.start.line}
                    break

    # ------------------------------------------------------------------ walk
    def parse(self, path: Path, idx: ci.Index) -> None:
        self.tu = dx.rel(str(path))
        tu = idx.parse(str(path), args=dx.ARGS)
        stack: list[ci.Cursor] = []

        def walk(c: ci.Cursor) -> None:
            stack.append(c)
            k = c.kind
            if k == K.CSTYLE_CAST_EXPR:
                self.cast(stack)
            elif k == K.CALL_EXPR:
                self.call(stack)
            for ch in c.get_children():
                if ch.location.file is None or dx.origin(dx.rel(ch.location.file.name)) in ("game", "gen") or \
                        ch.kind != K.FUNCTION_DECL:
                    walk(ch)
            stack.pop()

        sys.setrecursionlimit(20000)
        for top in tu.cursor.get_children():
            if top.location.file is None or dx.origin(dx.rel(top.location.file.name)) != "game":
                continue
            walk(top)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    a = ap.parse_args()
    index = json.loads((dx.REPO / "build" / "port" / "index.json").read_text(encoding="utf-8"))
    t0 = time.time()
    hz = Hazards(rx.RefIndex(rx.Ledgers(index), index["macros"]))
    idx = ci.Index.create()
    files = dx.game_tus(a.only)
    for p in files:
        hz.parse(p, idx)
    dests = collections.Counter(re.sub(r":.*", "", c["dest"]) for c in hz.casts.values())
    out = {"ptr_casts": sorted(hz.casts.values(), key=lambda c: (c["file"], c["start"])),
           "decls": hz.decls, "size_literals": sorted(hz.sizes.values(), key=lambda c: (c["file"], c["start"])),
           "stats": {"casts": len(hz.casts), "by_dest": dict(dests.most_common()), "decls": len(hz.decls),
                     "size_literals": len(hz.sizes), "skipped": dict(hz.skipped),
                     "seconds": round(time.time() - t0)}}
    OUT.write_text(json.dumps(out, indent=0), encoding="utf-8")
    print(json.dumps(out["stats"], indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
