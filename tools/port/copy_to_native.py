"""Copy the named decomp into Native\\ (plan Step 4).

Every function, global, type and macro is moved by byte range (build/port/decls.json) to its ledger
`final_location`, and every identifier inside it is renamed by position (build/port/refs.json), never by text
search. The output is plain C17 (`.c`/`.h`); it is promoted to C++ later (plan §8).

  functions  FUNCTION_CATALOG.csv rows with copy = yes. dup-of rows are copied once (the row they point at).
             Rows with no C body (asm-only, INCLUDE_ASM, inline asm) become stubs that call VS_UNPORTED
             (PORTING_WATCHLIST.md §5). Effect template rows are copied by copy_templates (one shared body per
             template, each instance a binding).
  globals    DATA_LEDGER.csv: the C definition when the decomp has one, otherwise a zero definition from the
             extern declaration (bss, or data that Step 5 loads from the disc). Function-local statics stay in
             their function. File statics get external linkage, since a module's functions are now spread over
             several files.
  types      TYPE_LEDGER.csv: written to the type's header as `typedef struct Name {...} Name;` (anonymous
             structs get the tag). same-as rows are written once, as the row they point at.
  macros     game macros the copied text uses, written to the header of the file that uses them most.

Each `X.c` gets a sibling `X.h` with its prototypes and extern declarations. Headers forward-declare the structs
they only point to and include the headers of everything else they use; `.c` files include every header that
provides a name they use. Includes are spelled relative to a project's `src\\` or `include\\` folder.

The output is regenerated from the decomp until each item is modernized. Two safeguards keep hand work safe:

  owned items  tools\\port\\owned.csv lists functions/globals/types whose code is now hand-maintained (moved out
               by tools\\port\\own.py into a file the copy tool never writes). They are not emitted; their names
               are provided by the owned header listed there, and their ledger `modernized` column reads `wip`
               (`x` once the rewrite tool approves them).
  manifest     tools\\port\\copy_manifest.json (committed) records the hash of every generated file. If a
               generated file was edited by hand, or a file to be written exists and is not generated, the run
               stops without writing anything: take ownership with own.py (or revert the edit) instead.

  python tools\\port\\copy_to_native.py [--report] [--mark]
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import copy_fixups as cf  # noqa: E402
import decomp_index as dx  # noqa: E402
import native_views as nv  # noqa: E402
from ref_index import RESIDENT, overlay_of  # noqa: E402
from ref_index import norm_key as rx_norm  # noqa: E402

REPO = dx.REPO
LEDGERS = REPO / "Docs" / "ledgers"
PORT = REPO / "build" / "port"
MANIFEST = REPO / "tools" / "port" / "copy_manifest.json"
OLD_MANIFEST = PORT / "copy_manifest.json"
OWNED = REPO / "tools" / "port" / "owned.csv"
NATIVE = REPO / "Native"
LEGACY_HEADER = "Legacy/Psx.h"
IDENT = re.compile(r"[A-Za-z_]\w*")

# Strategy R (SDK_API_LEDGER.md): SDK calls rewritten to the native API at copy time.
GTE_FUNCS = {n: f"Core_Gte_{n}" for n in (
    "MulMatrix", "CompMatrix", "ApplyMatrix", "ApplyRotMatrix", "ApplyMatrixLV", "ApplyMatrixSV", "TransMatrix",
    "ScaleMatrix", "RotTransPers", "RotTrans", "RotTransPers4", "RotAverage4", "NormalClip", "OuterProduct0",
    "VectorNormal", "VectorNormalS", "VectorNormalSS", "SquareRoot12", "DpqColor", "SetRotMatrix", "SetTransMatrix", "SetGeomOffset",
    "SetGeomScreen", "SetFarColor", "SetFogNear")}
SDK_RENAMES = {
    **GTE_FUNCS,
    "RotMatrix_gte": "Core_Gte_RotMatrix", "RotMatrixYXZ_gte": "Core_Gte_RotMatrixYXZ",
    "rsin": "Core_Gte_Sin", "rcos": "Core_Gte_Cos", "ratan2": "Core_Gte_Atan2",
    "rand": "Core_Random_Rand", "srand": "Core_Random_Seed", "index": "strchr",
    # libapi file calls collide with C runtime names
    **{n: f"LegacyApi_{n}" for n in ("open", "close", "read", "write", "erase", "rename", "cd", "firstfile", "nextfile")},
}
# Inline GTE macros (SDK inline_c.h and the game's vs_inline_c.h, whose bodies are MIPS assembly) and the scratch
# address macro, rewritten by token.
TOKEN_RENAMES = {
    "gte_SetRotMatrix": "Core_Gte_SetRotMatrix", "gte_SetTransMatrix": "Core_Gte_SetTransMatrix",
    "gte_SetTransVector2": "Core_Gte_SetTransVector", "gte_ldv0": "Core_Gte_LoadV0", "gte_ldsxy3": "Core_Gte_LoadSxy3",
    "gte_rtps": "Core_Gte_Rtps", "gte_rtps2": "Core_Gte_Rtps", "gte_rtv0tr": "Core_Gte_Rtv0Tr",
    "gte_rtv0tr2": "Core_Gte_Rtv0Tr", "gte_nclip": "Core_Gte_Nclip", "gte_nclip2": "Core_Gte_Nclip",
    "gte_zrtr": "Core_Gte_ClearTranslation", "gte_stsxy": "Core_Gte_StoreSxy2", "gte_stszotz": "Core_Gte_StoreSz3Otz",
    "gte_stotz": "Core_Gte_StoreOtz", "gte_stopz": "Core_Gte_StoreOpz", "gte_stlvnl": "Core_Gte_StoreLvnl",
    "gte_stsv": "Core_Gte_StoreIr",
    "getScratchAddr": "Core_ScratchAddr",
}
TOKEN_RX = re.compile(r"\b(" + "|".join(sorted(TOKEN_RENAMES, key=len, reverse=True)) + r")\b")
# Strategy D: calls deleted when they are a whole statement (Legacy/Removed.h keeps the rest compiling).
REMOVED = set(re.findall(r"static inline [\w\s\*]+?\b(\w+)\(",
                         (NATIVE / "Engine" / "Legacy" / "Removed.h").read_text(encoding="utf-8")))
# The game's scratch block (vs_scratch) is the start of the PS1 scratch memory, which getScratchAddr(n) also reaches.
SCRATCH_GLOBAL = "G00011"


def delete_removed_calls(text: str) -> tuple[str, int]:
    out, pos, n = [], 0, 0
    for m in re.finditer(r"(?m)^[ \t]*(\w+)\s*\(", text):
        if m[1] not in REMOVED or m.start() < pos:
            continue
        depth, i = 0, m.end() - 1
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            if depth == 0:
                break
            i += 1
        j = i + 1
        while j < len(text) and text[j] in " \t":
            j += 1
        if j < len(text) and text[j] == ";":
            # only a whole statement: after `;`, `{`, `}` or a label. After `if (...)` / `else` it becomes `;`;
            # anywhere else (an assignment, an argument) the call stays and Legacy/Removed.h answers it.
            before = text[:m.start()].rstrip()
            prev = before[-1:] if before else "{"
            end = j + 1
            if text[end:end + 1] == "\n":
                end += 1
            if prev in ";{}:":
                replacement = ""
            elif prev == ")" or before.endswith("else"):
                replacement = text[m.start():m.start() + len(m[0]) - len(m[0].lstrip())] + ";\n"
            else:
                continue
            out.append(text[pos:m.start()])
            out.append(replacement)
            pos, n = end, n + 1
    out.append(text[pos:])
    return "".join(out), n


def finish_text(text: str) -> str:
    text = TOKEN_RX.sub(lambda m: TOKEN_RENAMES[m[1]], text)
    # string-table data the decomp splices in from its disc build (#include "build/assets/...vsString")
    text = re.sub(r'#\s*include\s+"build/assets/[^"]+\.vsString"', "0 /* disc text table: loaded in Step 5 */", text)
    # scratch memory addresses (0x1F800000-0x1F8003FF) are Core's scratch block (Core/Scratch.h)
    text = SCRATCH_RX.sub(lambda m: f"((uintptr_t)Core_ScratchBase() + 0x{m[1].upper()})", text)
    # SPU registers (0x1F801C00-0x1F801DFF) are the libspu shim's register block (Legacy/Audio/LibSpu.h)
    return SPU_REGISTER_RX.sub(lambda m: f"((uintptr_t)LegacySpu_Registers() + 0x{int(m[1], 16) - 0xC00:03X})", text)


SCRATCH_RX = re.compile(r"\b0x1[Ff]800([0-3][0-9A-Fa-f]{2})\b")
SPU_REGISTER_RX = re.compile(r"\b0x1[Ff]801([CcDd][0-9A-Fa-f]{2})\b")


# Macros that select code in shared headers (`#ifdef _ITEMMENU` in MENU/items2.h is set by the including .c) or
# that the decomp build defines on the command line. Their #ifdef blocks are resolved per translation unit when a
# function is copied, since the native file no longer has the including file's context.
CONFIG_MACROS = {"_ITEMMENU", "MENUBGSZ"}
BUILD_DEFINES = {"MENUBGSZ"}


def resolve_conditionals(text: str, defined: set[str]) -> str:
    out, stack = [], []   # stack of (keep_this_branch, parent_active, handled)
    active = True
    for line in text.splitlines(keepends=True):
        m = re.match(r"\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b\s*(\w*)", line)
        if m and (m[1] in ("else", "endif") and stack and stack[-1][2] or
                  m[1] in ("ifdef", "ifndef") and m[2] in CONFIG_MACROS):
            if m[1] in ("ifdef", "ifndef"):
                cond = (m[2] in defined) == (m[1] == "ifdef")
                stack.append((cond, active, True))
                active = active and cond
            elif m[1] == "else":
                cond, parent, _ = stack[-1]
                stack[-1] = (not cond, parent, True)
                active = parent and not cond
            else:
                _, active, _ = stack.pop()
            continue
        if m and m[1] in ("ifdef", "ifndef", "if"):
            stack.append((True, active, False))  # an unrelated conditional: kept as written
        elif m and m[1] == "endif" and stack:
            stack.pop()
        if active:
            out.append(line)
    return "".join(out)


def rd(name: str) -> list[dict]:
    with (LEDGERS / name).open(encoding="utf-8", newline="") as fh:
        return list(csv.DictReader(fh))


def c_name(final: str) -> str:
    return final.strip().replace("::", "_")


def note(row: dict, key: str) -> str | None:
    m = re.search(rf"(?:^|;)\s*{key}:(\w+)", row.get("notes") or "")
    return m[1] if m else None


def strip_code(text: str) -> str:
    """Comments and string/char literals blanked, so identifier scans see only code."""
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', " ", text, flags=re.S)


def include_spelling(path: str) -> str:
    """Native\\Game\\src\\Menu\\Files.h -> Menu/Files.h (relative to the project's src or include folder)."""
    parts = path.split("\\")
    for root in ("src", "include"):
        if root in parts:
            return "/".join(parts[parts.index(root) + 1:])
    raise ValueError(path)


def sibling_header(path: str) -> str:
    return path[:-2] + ".h" if path.endswith(".c") else path


def splat_asset_extents(known: set[tuple[str, str]] | None = None) -> dict[tuple[str, str], tuple[int, int]]:
    """(module, symbol) -> (file offset, bytes) for every named subsegment in the decomp's splat configs. Asset
    symbols (images, palettes) have no C type, so their extent is from their start to the next subsegment that is a
    boundary: code or data, or an asset the C code declares itself (`known`, (module, name) pairs). Splat may cut one
    C array into several images (`_fontTable[]` is `_fontTable` and `_fontTable1`); those stay one extent. An
    `indexed` image keeps its palettes first unless `cluts_after: true`: the segment start is then `<name>_clut`
    and `<name>` is the pixels after the palettes (num_cluts, default 1, of 16 or 256 entries by bitdepth)."""
    out: dict[tuple[str, str], tuple[int, int]] = {}
    for cfg in sorted((dx.DECOMP / "config").rglob("splat.yaml")):
        text = cfg.read_text(encoding="utf-8")
        m = re.search(r"^\s*basename:\s*(\S+)", text, re.M)
        if not m:
            continue
        module = m[1]
        starts: list[tuple[int, str | None]] = []
        sized: set[str] = set()   # rgb555Sized: a 4-byte width/height header, then the pixels
        clut_first: dict[str, int] = {}   # indexed image -> bytes of palettes before its pixels
        for ln in text.splitlines():
            ln = ln.strip()
            if not ln.startswith("- "):
                continue
            b = re.match(r"-\s*\[\s*(0x[0-9A-Fa-f]+|\d+)\s*(?:,\s*([^,\]]+))?(?:,\s*([^,\]]+))?", ln)
            if b:
                kind = (b[2] or "").strip()
                name = (b[3] or "").strip() if kind and not kind.startswith(".") and kind not in ("c", "asm", "hasm",
                                                                                                    "bin") else None
                if name and kind == "rgb555Sized":
                    sized.add(name)
                starts.append((int(b[1], 0), name))
                continue
            d = re.match(r"-\s*\{\s*start:\s*(0x[0-9A-Fa-f]+|\d+)", ln)
            if d:
                n = re.search(r"\bname:\s*([\w]+)", ln)
                if n and re.search(r"\btype:\s*indexed\b", ln) and not re.search(r"\bcluts_after:\s*true", ln):
                    cluts = re.search(r"\bnum_cluts:\s*(\d+)", ln)
                    depth = re.search(r"\bbitdepth:\s*(\d+)", ln)
                    entries = 256 if depth and depth[1] == "8" else 16
                    clut_first[n[1]] = (int(cluts[1]) if cluts else 1) * entries * 2
                starts.append((int(d[1], 0), n[1] if n else None))
        def boundary(name: str | None, module_name: str = module) -> bool:
            if name is None or known is None:
                return True
            return any((module_name, n) in known for n in (name, name + "_clut", name + "_header", name + "_data"))

        for k, (start, name) in enumerate(starts[:-1]):
            nxt = next((s for s, n in starts[k + 1:] if boundary(n)), starts[-1][0])
            if name and nxt > start:
                if name in clut_first:
                    cb = clut_first[name]
                    out[(module, name + "_clut")] = (start, nxt - start)
                    out[(module, name)] = (start + cb, nxt - start - cb)
                else:
                    out[(module, name)] = (start, nxt - start)
                    out.setdefault((module, name + "_clut"), (start, nxt - start))
                if name in sized:
                    out[(module, name + "_header")] = (start, 4)
                    out[(module, name + "_data")] = (start + 4, nxt - start - 4)
    return out


# --------------------------------------------------------------------------------------------- source text + renames
class Source:
    def __init__(self, refs: dict) -> None:
        self.refs = refs["files"]
        self.extra: dict[str, list] = {}   # file -> [(offset, length, "edit", replacement)] (x64 hazard edits)
        self.bytes: dict[str, bytes] = {}
        self.line_starts: dict[str, list[int]] = {}
        self.ref_offsets: dict[str, list[tuple[int, int, str, str]]] = {}

    def load(self, rel: str) -> bytes:
        if rel not in self.bytes:
            p = dx.SRC / rel[4:] if rel.startswith("src/") else (
                dx.GEN / rel[4:] if rel.startswith("gen/") else dx.DECOMP / rel)
            b = p.read_bytes()
            self.bytes[rel] = b
            starts = [0]
            for i, ch in enumerate(b):
                if ch == 10:
                    starts.append(i + 1)
            self.line_starts[rel] = starts
            occ = []
            for line, col, n, kind, target in self.refs.get(rel, []):
                occ.append((starts[line - 1] + col - 1, n, kind, target))
            occ += self.extra.get(rel, [])
            occ.sort(key=lambda o: (o[0], o[1]))
            self.ref_offsets[rel] = occ
        return self.bytes[rel]

    def offset(self, rel: str, line: int) -> int:
        self.load(rel)
        return self.line_starts[rel][line - 1]

    def line_end(self, rel: str, off: int) -> int:
        b = self.load(rel)
        e = b.find(b"\n", off)
        return len(b) if e < 0 else (e - 1 if b[e - 1:e] == b"\r" else e)


@dataclass
class Item:
    kind: str            # function | proto | var | extern | type | forward | macro
    text: str
    name: str = ""       # C name provided
    row: str = ""        # ledger id
    need: list = field(default_factory=list)   # type ids needed complete
    order: tuple = (0,)
    src: str = ""        # decomp file the text came from (for that file's own macros)


@dataclass
class OutFile:
    path: str
    items: list = field(default_factory=list)

    @property
    def is_header(self) -> bool:
        return self.path.endswith(".h")


class Copier:
    def __init__(self) -> None:
        self.cat = [r for r in rd("FUNCTION_CATALOG.csv") if r["copy"] in ("yes", "data")]
        self.cat_by_id = {r["id"]: r for r in rd("FUNCTION_CATALOG.csv")}
        self.data = rd("DATA_LEDGER.csv")
        self.data_by_id = {r["id"]: r for r in self.data}
        self.types = rd("TYPE_LEDGER.csv")
        self.type_by_id = {r["id"]: r for r in self.types}
        self.fields = {r["id"]: r for r in rd("FIELD_LEDGER.csv")}
        decls = json.loads((PORT / "decls.json").read_text(encoding="utf-8"))
        self.decls = decls["decls"]
        self.macro_defs = decls["macros"]
        self.src = Source(json.loads((PORT / "refs.json").read_text(encoding="utf-8")))
        self.by_target: dict[str, list[dict]] = collections.defaultdict(list)
        for d in self.decls:
            for t in d["targets"]:
                self.by_target[t].append(d)
        self.files: dict[str, OutFile] = {}
        self.provider: dict[str, str] = {
            "Render_ModelWorkspace": r"Native\Engine\include\Render\ModelWorkspace.h",
            "Battle_Ai_FrameState": r"Native\Game\src\Battle\Ai\FrameUpdate.h",
        }      # C name -> header path that declares it
        self.struct_names: set[str] = set()     # C names of struct/union types (forward-declarable)
        self.type_home: dict[str, str] = {}     # type id (canonical) -> header path
        self.report: dict[str, list] = collections.defaultdict(list)
        self.asset_extents = splat_asset_extents({(r["overlay"], r["name"]) for r in self.data})
        self.string_asset_extents = splat_asset_extents()
        self.string_asset_sources: dict[str, tuple[str, int]] = {}
        self.module_data: list[tuple[dict, str, int | None]] = []   # (row, C name, bytes if unsized) filled at boot
        self.module_pointers: list[tuple[dict, str, int]] = []      # (row, C name, element count) relocated at boot
        self.deleted_calls = 0
        self.type_edges: dict[str, set[str]] = {}
        self.macro_body: dict[str, str] = {}
        self.public_headers = {r["final_location"].strip() for r in self.types if "\\include\\" in r["final_location"]}
        self.emitted: dict[str, dict] = {}   # ledger id -> declaration the native declaration was written from
        self._casts: dict = {}
        self.item_row: dict[str, str] = {}   # C name of a global/function -> ledger id
        self.type_deps: dict[str, list] = {}  # type id -> types it holds or points to
        self.used_fixups: set = set()
        self.plugin_ref: dict[int, str] = {}
        # MENUBGSZ: the decomp build passes the size of the title background asset (config/TITLE/TITLE.PRG/Makefile)
        bg = self.data_by_id.get("G02020", {})
        self.menubgsz = f"{bg.get('size') or 0} /* MENUBGSZ: size of {bg.get('final_name', '')} */"
        self.reset_ids = self.reset_rows()
        self.resets: dict[tuple[str, str], list] = collections.defaultdict(list)
        self.local_static_ids: dict[str, dict[str, list]] = collections.defaultdict(lambda: collections.defaultdict(list))
        for gid in self.reset_ids:
            g = self.data_by_id[gid]
            if g["linkage"] == "local-static":
                place = g["key"].split("@", 1)[1]          # src/MENU/MAINMENU.PRG/C48.c:function
                file, _, fn = place.rpartition(":")
                self.local_static_ids[file.removeprefix("src/")][fn].append(gid)
        self.global_names = {c_name(g["final_name"]) for g in self.data if g["linkage"] != "local-static"}
        # names that exist only inside a function body: local statics, and enumerators of function-local enums
        self.function_local_names = {c_name(g["final_name"]) for g in self.data if g["linkage"] == "local-static"}
        local_enums = {t["id"] for t in self.types if t["kind"] == "enum" and self.inline_type(t) == "local"}
        for f in self.fields.values():
            if f["type_id"] in local_enums:
                self.function_local_names.add(self.field_name(f["id"]))
        self.owned: dict[str, dict] = {}
        if OWNED.exists():
            with OWNED.open(encoding="utf-8", newline="") as fh:
                self.owned = {r["id"]: r for r in csv.DictReader(fh)}
        self.load_hazards()

    # ------------------------------------------------------------------------------------------------- x64 hazards
    BASE_INT = r"(?:unsigned\s+long|unsigned\s+int|signed\s+int|unsigned|long|int|u_long|u_int|uint|ulong)"
    SCALAR = r"(?:(?:un)?signed\s+)?(?:char|short|int|long|u_char|u_short|u_int|u_long)"

    SIGNED_INTS = re.compile(r"^\s*(signed\s+)?(int|long|short|char|s8|s16|s32|long\s+int)\s*$")

    def pointer_int(self, original: str) -> str:
        """The pointer-sized integer standing in for a cast or local that held a pointer: signed where the original
        type was, so comparisons such as `(long)result < 0` (an error sentinel in a pointer) keep their meaning."""
        return "intptr_t" if self.SIGNED_INTS.match(original) else "uintptr_t"

    def load_hazards(self) -> None:
        """Edits from tools/port/hazard_index.py (PORTING_WATCHLIST.md §2): pointer-to-integer casts become
        (uintptr_t), ordering-table links become LegacyGpu_Handle(p), and the members, globals and locals those
        values are stored into become uintptr_t, unless the member is part of an on-disc layout (decoder work).
        Struct-size literals of structs that hold pointers become sizeof."""
        self.ptr_fields: set[str] = set()
        self.ptr_globals: set[str] = set()
        self.sig_retype: dict[tuple[str, int | str], str] = {}   # (function id, "ret" | param index) -> type
        path = PORT / "hazards.json"
        if not path.exists():
            self.report["hazards-not-indexed"].append("run tools/port/hazard_index.py")
            return
        h = json.loads(path.read_text(encoding="utf-8"))
        edits: dict[str, list] = collections.defaultdict(list)
        left = ("deref", "return", "param", "param-local", "array_subscript_expr", "unary_operator")
        for c in h["ptr_casts"]:
            d = c["dest"]
            kind = d.split(":", 1)[0]
            fixed = True  # whether the value lands somewhere pointer-sized after the edits
            sig = re.match(r"(return|param):(F\d{5})(?::(\d+))?$", d)
            if sig:
                # the function's return value or parameter carries the pointer: it becomes pointer-sized in every
                # declaration (retype_signature), and the cast with it
                cast = re.match(r"\(([^)]*)\)", c["text"])
                self.sig_retype[(sig[2], "ret" if sig[1] == "return" else int(sig[3]))] = \
                    self.pointer_int(cast[1] if cast else "")
            elif kind == "field" and d != "field:?":
                fid = re.sub(r"~w\d+$", "", d[6:]).split(">")[-1]
                tid, _, idx = fid.partition(".")
                t = self.type_by_id.get(tid)
                f = self.fields.get(fid)
                if t is None or f is None:
                    fixed = False
                elif "*" in f["type"]:
                    pass  # already a pointer: the (uintptr_t) value converts back without truncation
                elif t["role"] == "OnDisc":
                    self.report["hazard-on-disc-member"].append(f"{fid} {t['final_name']}.{f['final_name']}")
                    fixed = False
                else:
                    self.ptr_fields.add(f"{self.canon_type(tid)}.{idx}")
            elif kind == "global":
                if d == "global:?":
                    fixed = False
                elif "*" not in self.data_by_id.get(d[7:], {}).get("type", ""):
                    self.ptr_globals.add(d[7:])  # (a pointer-typed global needs nothing: the value converts back)
            elif kind == "local" and d in h["decls"]:
                s = h["decls"][d]
                edits[s["file"]].append((s["start"], s["end"] - s["start"], "edit", self.pointer_int(s["text"])))
            elif kind in left or d == "field:?":
                # left as written, so the compiler keeps reporting it (signature or decoder work, §2)
                self.report[f"hazard-left-{kind}"].append(f"{c['file']}:{c['line']} {c['text']}")
                fixed = False
            if d == "link":
                edits[c["file"]].append((c["start"], c["close"] - c["start"], "edit", "LegacyGpu_Handle("))
                edits[c["file"]].append((c["operand_end"], 0, "edit", ")"))
            elif fixed:
                cast = re.match(r"\(([^)]*)\)", c["text"])
                edits[c["file"]].append((c["start"], c["close"] - c["start"], "edit",
                                         f"({self.pointer_int(cast[1] if cast else '')})"))
        for s in h["size_literals"]:
            t = self.type_by_id.get(self.canon_type(s["type"]))
            if t is None:
                continue
            size = f"sizeof({c_name(t['final_name'])})"
            text = size if s["count"] == 1 else f"{s['count']} * {size}"
            edits[s["file"]].append((s["start"], s["end"] - s["start"], "edit", text))
        for rel, es in edits.items():
            self.src.extra[rel] = sorted(set(es))
        for fid, sig in getattr(cf, "SIG_RETYPE", {}).items():   # reviewed additions (pointer returns not cast)
            for k, t in sig.items():
                self.sig_retype[(fid, k)] = t
        for rel, es in self.ondisc_edits().items():
            self.src.extra[rel] = sorted(set(self.src.extra.get(rel, []) + es))

    def ondisc_edits(self) -> dict[str, list]:
        """`E->m` / `E.m` for the members in copy_fixups.ONDISC_OFFSETS -> VS_ONDISC(E, m, T) (Legacy/OnDisc.h): the
        member holds the file's offset from the struct, and the use gets the pointer it stood for."""
        edits: dict[str, list] = collections.defaultdict(list)
        ident = set(b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")

        def back_ws(b: bytes, j: int) -> int:
            while j > 0 and b[j - 1] in b" \t\r\n":
                j -= 1
            return j

        def match_back(b: bytes, close_at: int) -> int:
            close = b[close_at]
            opener = ord("(") if close == ord(")") else ord("[")
            depth = 0
            for k in range(close_at, -1, -1):
                if b[k] == close:
                    depth += 1
                elif b[k] == opener:
                    depth -= 1
                    if depth == 0:
                        return k
            return 0

        def base_start(b: bytes, end: int) -> int:
            j = end
            while True:
                j = back_ws(b, j)
                if j == 0:
                    break
                c = b[j - 1]
                if c == ord("]"):
                    j = match_back(b, j - 1)
                    continue
                if c == ord(")"):
                    j = match_back(b, j - 1)
                    k = back_ws(b, j)
                    if k and b[k - 1] in ident:   # a call: include the callee
                        while k and b[k - 1] in ident:
                            k -= 1
                        j = k
                elif c in ident:
                    while j and b[j - 1] in ident:
                        j -= 1
                else:
                    break
                k = back_ws(b, j)
                if k >= 2 and b[k - 2:k] == b"->":
                    j = k - 2
                    continue
                if k >= 1 and b[k - 1:k] == b".":
                    j = k - 1
                    continue
                break
            return back_ws(b, end) if j == end else j

        for rel, occ in self.src.refs.items():
            hits = [o for o in occ if o[3] in ("field", "field@macro") and o[4] in cf.ONDISC_OFFSETS]
            if not hits:
                continue
            p = dx.SRC / rel[4:] if rel.startswith("src/") else (dx.GEN / rel[4:] if rel.startswith("gen/")
                                                                 else dx.DECOMP / rel)
            b = p.read_bytes()
            starts = [0] + [i + 1 for i, ch in enumerate(b) if ch == 10]
            for line, col, n, kind, fid in hits:
                off = starts[line - 1] + col - 1
                k = back_ws(b, off)
                arrow = k - 2 if b[k - 2:k] == b"->" else (k - 1 if b[k - 1:k] == b"." else None)
                if arrow is None:
                    continue  # the member's declaration
                if kind != "field":
                    self.report["ondisc-in-macro"].append(f"{rel}:{line} {fid}")
                    continue
                start = base_start(b, arrow)
                f = self.fields[fid]
                ctype = self.spelled_type(f["type"]).strip()
                if "*" not in ctype:
                    ctype = "void*"   # stored as an integer in the decomp's view
                macro = "VS_ONDISC" if b[arrow:arrow + 1] == b"-" else "VS_ONDISC_DOT"
                edits[rel].append((start, 0, "edit", f"{macro}("))
                edits[rel].append((arrow, off + n - arrow, "edit", f", {f['final_name']}, {ctype})"))
                self.report["ondisc-use"].append(f"{rel}:{line} {fid}")
        return edits

    def retype_ondisc(self, tid: str, text: str) -> str:
        """On-disc offset members are declared as the file has them: 32-bit offsets."""
        for fid in sorted(f for f in cf.ONDISC_OFFSETS if f.split(".")[0] == tid):
            name = self.fields[fid]["final_name"]
            text, n = re.subn(rf"(?m)^(\s*(?:/\*[^*]*\*/\s*)?)[\w\s]+?\*+\s*({re.escape(name)}\s*;)",
                              r"\1u_int \2 /* offset from the struct (on-disc pointer) */", text)
            self.report["ondisc-member" if n else "ondisc-member-not-retyped"].append(f"{fid} {name}")
        return text

    def retype_members(self, tid: str, text: str) -> str:
        for fid in sorted(f for f in self.ptr_fields if f.split(".")[0] == tid):
            name = self.fields[fid]["final_name"]
            text, n = re.subn(rf"(?m)^(\s*(?:/\*[^*]*\*/\s*)?(?:volatile\s+)?){self.BASE_INT}(\s+{re.escape(name)}"
                              rf"\s*(?:\[[^\]]*\]\s*)*;)", r"\1uintptr_t\2", text)
            self.report["hazard-member-uintptr" if n else "hazard-member-not-retyped"].append(
                f"{fid} {c_name(self.type_by_id[tid]['final_name'])}.{name}")
        return text

    def retype_global(self, gid: str, name: str, text: str) -> str:
        new, n = re.subn(rf"\b{self.BASE_INT}(\s+(?:const\s+)?{re.escape(name)}\b)", r"uintptr_t\1", text, count=1)
        if gid in self.ptr_globals:
            self.report["hazard-global-uintptr" if n else "hazard-global-not-retyped"].append(f"{gid} {name}")
        return new if n else text

    def take_owned(self, rid: str, name: str) -> bool:
        """An owned item is not emitted; its name is provided by the owned header."""
        o = self.owned.get(rid)
        if o is None:
            return False
        hdr = o["header"].strip()
        if not (REPO / hdr).exists():
            raise SystemExit(f"owned.csv: {rid} {name}: header {hdr} does not exist")
        self.provide(name, hdr)
        self.item_row[name] = rid
        self.report["owned"].append(f"{rid} {name} -> {o['path']}")
        return True

    # ------------------------------------------------------------------------------------------------- naming
    def canon_type(self, tid: str) -> str:
        seen = set()
        while tid in self.type_by_id and tid not in seen:
            seen.add(tid)
            s = note(self.type_by_id[tid], "same-as") or cf.NESTED_SAME_AS.get(tid)
            if not s or s not in self.type_by_id:
                break
            tid = s
        return tid

    def field_name(self, fid: str) -> str:
        r = self.fields[fid]
        t = self.type_by_id.get(r["type_id"])
        canon = self.canon_type(r["type_id"])
        if canon != r["type_id"]:
            # same-as types have one layout; the copy writes the canonical type, so use its member at that index
            cf_ = self.fields.get(f"{canon}.{fid.split('.')[1]}")
            if cf_ is not None:
                r = cf_
        if t and t["kind"] == "enum":
            return f"{c_name(self.type_by_id[canon]['final_name'])}_{r['final_name']}"
        return r["final_name"]

    def target_text(self, target: str) -> str | None:
        target = re.sub(r"~w\d+$", "", target)
        if ">" in target:
            parts = []
            for p in target.split(">"):
                parts.append(p.rsplit(".", 1)[1] if p.startswith("sdk:") else self.target_text(p))
            return ".".join(parts)
        if target.startswith("sdk:"):
            return SDK_RENAMES.get(target[4:], target[4:])
        if target.startswith("plugin:"):
            return None
        if target.startswith("F"):
            r = self.cat_by_id[target]
            if r["copy"] not in ("yes", "data"):
                return r["name"]  # SDK/runtime code the decomp carries (copy = no): provided by Legacy
            return c_name(r["final_name"])
        if target.startswith("G"):
            return c_name(self.data_by_id[target]["final_name"])
        if re.fullmatch(r"T\d{5}", target):
            return c_name(self.type_by_id[self.canon_type(target)]["final_name"])
        if re.fullmatch(r"T\d{5}\.\d{3}", target):
            return self.field_name(target)
        return None

    @staticmethod
    def pick(target: str, overlay: str) -> str:
        if " " not in target:
            return target
        opts = [(t, set(ovs.split(","))) for t, _, ovs in (x.partition("|") for x in target.split())]
        for t, ovs in opts:
            if overlay in ovs:
                return t
        for res in RESIDENT:
            for t, ovs in opts:
                if res in ovs:
                    return t
        return opts[0][0]

    def renamed(self, rel: str, start: int, end: int, overlay: str, tu: str | None = None, own: str = "") -> str:
        """Text of [start, end) with every recorded identifier renamed. With `tu` (a function body), a global or
        function that this translation unit declared differently from the emitted declaration (decomp leniency:
        each file declared its own view) is used through a cast to that file's declaration."""
        b = self.src.load(rel)
        out, pos = [], start
        for off, n, kind, target in self.src.ref_offsets[rel]:
            if off < start or off + n > end or off < pos:
                continue
            if kind == "edit":
                out.append(b[pos:off])
                out.append(target.encode())
                pos = off + n
                continue
            t = self.pick(target, overlay)
            # An address shared with EFFPURGE's dev routine can be labelled F01889
            # in refs.json. The slot's plugin is authoritative, not that symbol.
            new = self.plugin_ref.get(off) if rel == self.EFFPURGE else None
            if new is None:
                new = self.target_text(t)
            if new is None:
                self.report["unrenamed"].append(f"{rel}@{off}: {target}")
                continue
            if tu and kind.split("@")[0] in ("func", "var") and t != own and re.fullmatch(r"[FG]\d{5}", t):
                ls = b.rfind(b"\n", 0, off) + 1
                if not re.match(rb"\s*extern\b", b[ls:off]):
                    new = self.view_cast(t, tu, new) or new
            out.append(b[pos:off])
            out.append(new.encode())
            pos = off + n
        out.append(b[pos:end])
        text = finish_text(b"".join(out).replace(b"\r\n", b"\n").decode("utf-8", errors="surrogateescape"))
        return self.rename_string_ids(text, overlay) if "VS_" in text else text

    @staticmethod
    def norm_ctype(t: str) -> str:
        t = re.sub(r"\[\d*\]", "[]", t)
        t = re.sub(r"\b(const|volatile)\b", "", t)
        return re.sub(r"\s+", "", t)

    def view_cast(self, target: str, tu: str, name: str) -> str | None:
        key = (target, tu)
        if key in self._casts:
            return self._casts[key]
        self._casts[key] = None
        emitted = self.emitted.get(target)
        kind = "function" if target.startswith("F") else "var"
        mine = [d for d in self.by_target.get(target, []) if d["kind"] == kind and tu in d["tus"]]
        mine.sort(key=lambda d: (d["file"] != tu, not d["def"], d["file"], d["start"]))
        unprototyped = any(x.get("ctype", "").endswith("()") for x in mine)
        if kind == "function" and emitted and not emitted.get("ctype", "").endswith("()") and (
                not mine or unprototyped):
            # no declaration in this file, or only an unprototyped `f()`: the original call compiled with whatever
            # arguments it passes; call through an unprototyped pointer to the real return type
            ov = overlay_of(emitted["file"]) if emitted["file"].startswith("src/") else "SLUS_010.40"
            head = self.head_of(self.drop_storage(self.renamed(emitted["file"], emitted["start"], emitted["end"], ov)))
            head = self.retype_signature(target, head, name)
            m = re.search(rf"\b{re.escape(name)}\s*\(", head)
            if m and "(*" not in head[:m.start()]:
                cast = f"(({head[:m.start()].strip()} (*)()){name})"
                self._casts[key] = cast
                self.report["implicit-call-cast"].append(f"{name} in {tu}")
                return cast
            return None
        if not emitted or not mine or "ctype" not in mine[0] or "ctype" not in emitted:
            return None
        d = mine[0]
        sig = lambda x: (self.norm_ctype(x["ctype"]),  # noqa: E731
                         sorted({self.canon_type(t) for t in x.get("need", []) + x.get("ptr", [])}))
        # the same typedef name can be a different struct in another file, so compare the ledger types as well
        if sig(d) == sig(emitted):
            return None
        ov = overlay_of(d["file"]) if d["file"].startswith("src/") else "SLUS_010.40"
        text = self.drop_storage(self.renamed(d["file"], d["start"], d["end"], ov))
        text = re.sub(r"^\s*extern\s+", "", text)
        if kind == "function":
            text = self.retype_signature(target, self.head_of(text).rstrip(";").strip(), name)
        else:
            text = self.split_init(self.own_declarator(d, text).strip())[0]
        abstract, k = re.subn(rf"\b{re.escape(name)}\b", "(*)", text, count=1)
        if not k:
            return None
        cast = f"(({abstract}){name})" if kind == "function" else f"(*({abstract})&{name})"
        self._casts[key] = cast
        self.report["view-cast"].append(f"{name} in {tu}")
        return cast

    # ------------------------------------------------------------------------------------------------- output
    def norm_header(self, path: str) -> str:
        if path.endswith(".h") and "\\src\\" in path:
            # a module with a public header (include\) and a private one of the same name: one header, the public
            public = path.replace("\\src\\", "\\include\\", 1)
            if public in self.public_headers:
                return public
        return path

    def out(self, path: str) -> OutFile:
        path = self.norm_header(path)
        if path not in self.files:
            self.files[path] = OutFile(path)
        return self.files[path]

    def provide(self, name: str, header: str) -> None:
        self.provider.setdefault(name, self.norm_header(header))

    def pick_decl(self, target: str, *, kind: str, definition: bool | None) -> dict | None:
        cands = [d for d in self.by_target.get(target, []) if d["kind"] == kind and
                 (definition is None or d["def"] == definition)]
        if not cands:
            return None
        cands.sort(key=lambda d: (d["file"].startswith("gen/"), d["file"], d["start"]))
        return cands[0]

    # ------------------------------------------------------------------------------------------------- functions
    @staticmethod
    def head_of(text: str) -> str:
        depth = 0
        for i, ch in enumerate(text):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            elif ch == "{" and depth == 0:
                return text[:i].rstrip()
        return text.rstrip()

    @staticmethod
    def drop_storage(text: str) -> str:
        return re.sub(r"^\s*((?:static|inline|INLINE|__inline__|__inline)\s+)+", "", text)

    def config_defined(self, d: dict, r: dict) -> set[str]:
        """Configuration macros defined in the translation unit this copy of the function was compiled in."""
        if not hasattr(self, "_tu_macros"):
            self._tu_macros = json.loads((PORT / "decls.json").read_text(encoding="utf-8"))["tu_macros"]
        ov = overlay_of("src/" + r["file"])
        tus = [t for t in d.get("tus", []) if overlay_of(t) == ov] or d.get("tus", [])
        names = set(BUILD_DEFINES)
        if tus:
            names |= set(self._tu_macros.get(tus[0], {})) & CONFIG_MACROS
        return names

    def apply_fixups(self, r: dict, text: str) -> str:
        for old, new, why in cf.FIXES.get(r["id"], []):
            if old not in text:
                raise SystemExit(f"copy_fixups: {r['id']} {r['final_name']}: '{old}' not found ({why})")
            text = text.replace(old, new)
            self.used_fixups.add((r["id"], old))
        for row_rx, pat, rep, _reason in cf.REGEX_FIXES:
            if re.search(row_rx, r["final_name"]):
                text, n = re.subn(pat, rep, text)
                if n:
                    self.used_fixups.add((row_rx, pat))
        return text

    def rename_leftovers(self, text: str, overlay: str) -> str:
        """Decomp names libclang never recorded (calls in its error translation units, e.g. 3A1A0.c calling
        func_800A4A24 before its definition): resolved through the catalog of the function's own module, then the
        resident ones, when exactly one final name matches."""
        if not hasattr(self, "_cat_ov"):
            self._cat_ov = collections.defaultdict(set)
            for r in self.cat:
                self._cat_ov[(r["overlay"], r["name"])].add(c_name(r["final_name"]))

        def sub(m: re.Match) -> str:
            for ov in (overlay, "BATTLE.PRG", "SLUS_010.40"):
                names = self._cat_ov.get((ov, m[0]))
                if names and len(names) == 1:
                    self.report["leftover-renamed"].append(f"{m[0]} -> {next(iter(names))}")
                    return next(iter(names))
            return m[0]
        return re.sub(r"\bfunc_[0-9A-F]{8}\b", sub, text)

    def retype_signature(self, fid: str, text: str, name: str) -> str:
        """Return type / parameters that carry pointers (hazards `return:`/`param:`) become pointer-sized."""
        keys = {k[1]: t for k, t in self.sig_retype.items() if k[0] == fid}
        if not keys:
            return text
        m = re.search(rf"\b{re.escape(name)}\s*\(", text)
        if m is None:
            return text
        head, rest = text[:m.start()], text[m.end():]
        if "ret" in keys:
            head, n = re.subn(rf"\b{self.BASE_INT}(\s*)$", lambda x: keys["ret"] + x[1], head, count=1)
            self.report["signature-return-retyped" if n else "signature-return-not-retyped"].append(f"{fid} {name}")
        depth, start, params = 0, 0, []
        for i, ch in enumerate(rest):
            if ch in "([":
                depth += 1
            elif ch in ")]":
                if depth == 0:
                    params.append(rest[start:i])
                    end = i
                    break
                depth -= 1
            elif ch == "," and depth == 0:
                params.append(rest[start:i])
                start = i + 1
        else:
            return head + text[m.start():m.end()] + rest
        for idx, t in keys.items():
            if idx == "ret" or idx >= len(params):
                continue
            new, n = re.subn(rf"^(\s*){self.BASE_INT}\b",
                             lambda match, native_type=t: match[1] + native_type, params[idx], count=1)
            params[idx] = new
            self.report["signature-param-retyped" if n else "signature-param-not-retyped"].append(f"{fid}:{idx} {name}")
        return head + text[m.start():m.end()] + ",".join(params) + rest[end:]

    def copy_functions(self) -> None:
        # declaration each function is emitted from (definition, else prototype), known before any body is copied
        for r in self.cat:
            src = r["id"]
            d = self.pick_decl(src, kind="function", definition=True)
            if d is None or r["kind"] not in ("C", "C-header"):
                d = self.pick_decl(src, kind="function", definition=False) or d
            if d is not None:
                self.emitted[src] = d
        for r in self.cat:
            dup = note(r, "dup-of")
            if dup and dup in self.emitted:
                self.emitted.setdefault(r["id"], self.emitted[dup])
        # an assembly-only row whose bytes equal a function that exists as C elsewhere (dup-of) is written from
        # that C (PORTING_WATCHLIST.md §5): F01920 (PLG052, INCLUDE_ASM) is the C of F01998 (PLG077)
        c_body: dict[str, dict] = {}
        for r in self.cat:
            dup = note(r, "dup-of")
            if (dup and r["kind"] in ("C", "C-header") and dup not in c_body
                    and self.cat_by_id[dup]["kind"] not in ("C", "C-header")):
                c_body[dup] = r
        for r in self.cat:
            if note(r, "dup-of") or r["kind"].startswith("template") or r["copy"] != "yes":
                continue
            loc = r["final_location"].strip()
            name = c_name(r["final_name"])
            if self.take_owned(r["id"], name):
                continue
            src_row = c_body.get(r["id"], r)
            ov = overlay_of("src/" + src_row["file"])
            prov = f"// decomp: {r['overlay']} {r['name']} (src/{r['file']}:{r['line']})"
            if src_row is not r:
                prov += (f"\n// C from {src_row['overlay']} {src_row['name']} (src/{src_row['file']}:{src_row['line']}),"
                         f" the same code (dup-of)")
                self.report["asm-row-from-c-duplicate"].append(f"{r['id']} <- {src_row['id']}")
            redirect = cf.NATIVE_FUNCTION_REDIRECTS.get(r["id"])
            if redirect:
                target, _source = redirect
                header = sibling_header(loc)
                self.out(header).items.append(Item(
                    "macro", f"{prov.replace(':0)', ')')}\n"
                    "// Native replacement: shared SDK implementation, not a second legacy body.\n"
                    f"#define {name} {target}\n", name, r["id"], [],
                    (2, r["file"], int(r["line"]))))
                self.macro_body[name] = target
                self.provide(name, header)
                self.report["native-api-redirect"].append(f"{r['id']} {name} -> {target}")
                continue
            d = self.pick_decl(src_row["id"], kind="function", definition=True)
            text = None
            if d is not None and src_row["kind"] in ("C", "C-header"):
                tu = d["file"] if d["file"].endswith(".c") else next(
                    (t for t in d["tus"] if overlay_of(t) == ov), (d["tus"] or [None])[0])
                text = self.drop_storage(self.renamed(d["file"], d["start"], d["end"], ov, tu=tu, own=src_row["id"]))
                if "__asm__" in text:
                    self.report["stub-inline-asm"].append(r["final_name"])
                    text = None
            if text is not None:
                text, n = delete_removed_calls(text)
                self.deleted_calls += n
                if re.search(r"#\s*if(n?def)?\b", text):
                    text = resolve_conditionals(text, self.config_defined(d, r))
                text = self.apply_fixups(r, text)
                text = self.retype_signature(r["id"], text, name)
                text = self.rename_leftovers(text, r["overlay"])
                text = self.hoist_local_statics(src_row, text, loc)
                if "MENUBGSZ" in text:
                    text = re.sub(r"\bMENUBGSZ\b", self.menubgsz, text)
                self.out(loc).items.append(Item("function", f"{prov}\n{text}\n", name, r["id"], d["need"],
                                                (2, r["file"], int(r["line"])), d["file"]))
                head = self.head_of(text)
            else:
                p = self.pick_decl(r["id"], kind="function", definition=False) or (
                    d if d is not None and r["kind"] in ("C", "C-header") else None)
                if p is None:
                    # never declared in C: callers compiled it as an implicit `int f()` declaration
                    head = f"int {name}()"
                    ret = "INT"
                    self.report["stub-no-prototype"].append(f"{r['id']} {r['final_name']}")
                    need = []
                else:
                    ptext = self.drop_storage(self.renamed(p["file"], p["start"], p["end"], ov))
                    head = (self.head_of(ptext) if p["def"] else ptext).rstrip(";").strip()
                    ret, need = p.get("ret_kind", "INT"), p["need"]
                    if p.get("variadic"):
                        self.report["stub-variadic"].append(r["final_name"])
                body = f'    VS_UNPORTED("{r["final_name"].strip()}");\n'
                head = self.retype_signature(r["id"], head, name)
                if ret == "RECORD":
                    body += f"    {p['ret']} result = {{0}};\n    return result;\n"
                elif ret != "VOID":
                    body += "    return 0;\n"
                self.out(loc).items.append(Item("function", f"{prov} [{r['kind']}: not yet ported]\n{head}\n{{\n"
                                                f"{body}}}\n", name, r["id"], need, (2, r["file"], int(r["line"]))))
            hdr = sibling_header(loc)
            self.out(hdr).items.append(Item("proto", f"{head};\n", name, r["id"],
                                            d["need"] if d else [], (4, r["file"], int(r["line"]))))
            self.provide(name, hdr)
            self.item_row[name] = r["id"]

    # ------------------------------------------------------------------------------------------------- globals
    @staticmethod
    def split_init(text: str) -> tuple[str, str | None]:
        depth = 0
        for i, ch in enumerate(text):
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "=" and depth == 0 and text[i + 1:i + 2] != "=" and text[i - 1:i] not in "!<>=":
                return text[:i].rstrip(), text[i + 1:].strip()
        return text.rstrip(), None

    def block_extern(self, gid: str) -> dict | None:
        """A global whose only declaration is a block-scope `extern` inside a function: that line, as a declaration."""
        for rel, rows in self.src.refs.items():
            for line, _column, _name, _kind, target in rows:
                if gid not in re.findall(r"[GFT]\d{5}(?:\.\d{3})?", target):
                    continue
                b = self.src.load(rel)
                s = self.src.offset(rel, line)
                e = self.src.line_end(rel, s)
                if re.match(rb"\s*extern\b[^;]*;\s*$", b[s:e]):
                    s += len(b[s:e]) - len(b[s:e].lstrip())
                    return {"file": rel, "start": s, "end": b.rfind(b";", s, e), "need": [], "line": line}
        return None

    def name_inline_type(self, text: str, d: dict) -> str:
        """`struct { ... } const x[] = ...`: the anonymous struct is a ledger type written to its header, so the
        variable refers to it by name (otherwise the extern in the header and the definition would be two types)."""
        m = re.match(r"(\s*(?:extern\s+|static\s+|const\s+|volatile\s+)*)(struct|union|enum)\s*{", text)
        if not m:
            return text
        body = next((x for x in self.decls if x["file"] == d["file"] and x["kind"] == "type" and x["targets"] and
                     d["start"] <= x["start"] <= d["start"] + len(m[1].encode()) + 16), None)
        if body is None:
            return text
        depth, i = 0, m.end() - 1
        while i < len(text):
            depth += {"{": 1, "}": -1}.get(text[i], 0)
            if depth == 0:
                break
            i += 1
        tname = c_name(self.type_by_id[self.canon_type(body["targets"][0])]["final_name"])
        return f"{m[1]}{m[2]} {tname}{text[i + 1:]}"

    @staticmethod
    def top_level_split(text: str, sep: str = ",") -> list[str]:
        parts, depth, start = [], 0, 0
        for i, ch in enumerate(text):
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == sep and depth == 0:
                parts.append(text[start:i])
                start = i + 1
        parts.append(text[start:])
        return parts

    def own_declarator(self, d: dict, text: str) -> str:
        """`extern u_char a[], b[], c[];`: libclang extents of b and c start at `extern` and end at their own name,
        so keep the type specifiers and only the last declarator."""
        sib = sorted((x for x in self.decls if x["file"] == d["file"] and x["start"] == d["start"] and
                      x["kind"] == "var"), key=lambda x: x["end"])
        parts = self.top_level_split(text)
        if len(sib) < 2 or len(parts) < 2:
            return text
        first = parts[0]
        m = re.search(r"[\s\*\(]*\b" + re.escape(self.target_text(sib[0]["targets"][0]) or sib[0]["name"]) + r"\b",
                      first)
        if not m:
            return text
        return f"{first[:m.start()].rstrip()} {parts[-1].strip()}"

    VIEW_RX = re.compile(r"(?:^|;)\s*view-of:(G\d{5})([+-](?:0x[0-9A-Fa-f]+|\d+))?")

    def plan_views(self) -> None:
        """Globals the decomp declares as views into another global (`view-of:BASE+OFF` in the data ledger): on the
        PS1 both names reach the same bytes, so natively the view is written in terms of its base. A view of
        array elements grows its base array to cover the view."""
        self.views: dict[str, tuple[str, int]] = {}
        self.view_base_min: dict[str, int] = {}
        for r in self.data:
            m = self.VIEW_RX.search(r["notes"] or "")
            if not m:
                continue
            base_id = m[1]
            b = self.data_by_id.get(base_id)
            if m[2] is not None:
                off = int(m[2], 0)
            elif b and r["address"] and b["address"]:
                off = int(r["address"], 0) - int(b["address"], 0)  # no offset written: the addresses give it
            else:
                off = 0
            self.views[r["id"]] = (base_id, off)
            dims_v, dims_b = re.findall(r"\[(\d+)\]", r["type"]), re.findall(r"\[(\d+)\]", b["type"] if b else "")
            if b and len(dims_v) == 1 and len(dims_b) == 1 and r["type"].split("[")[0] == b["type"].split("[")[0]:
                elem = int(b["size"]) // int(dims_b[0])
                if elem and off % elem == 0 and off >= 0:
                    need = off // elem + int(dims_v[0])
                    self.view_base_min[base_id] = max(self.view_base_min.get(base_id, 0), need)

    def view_macro(self, r: dict, name: str, decl: str) -> str | None:
        """The view as a macro over its base, or None if it cannot be expressed (reported)."""
        base_id, off = self.views[r["id"]]
        b = self.data_by_id.get(base_id)
        if b is None or base_id not in self.emitted:
            self.report["view-base-missing"].append(f"{r['id']} {r['final_name']} -> {base_id}")
            return None
        bname = c_name(b["final_name"])
        arr = re.search(rf"\b{re.escape(name)}\s*((?:\[[^\]]*\])+)", decl)
        vtype = decl[:arr.start()].strip() if arr else re.sub(rf"\b{re.escape(name)}\b", "", decl).strip()
        dims_b = re.findall(r"\[(\d+)\]", b["type"])
        if off < 0:
            self.report["view-negative-offset"].append(f"{r['id']} {r['final_name']} = {base_id}{off}")
            return None
        if arr and len(dims_b) == 1 and r["type"].split("[")[0] == b["type"].split("[")[0]:
            elem = int(b["size"]) // int(dims_b[0])
            if elem and off % elem == 0:
                return f"({bname} + {off // elem})"
        target = None
        tid = self.canon_type(b["type_id"]) if b["type_id"] else ""
        prefix, rem = bname, off
        if tid and "[" in b["type"]:
            elem = int(self.type_by_id.get(tid, {}).get("size") or 0)
            if elem > 0:
                prefix, rem = f"{bname}[{off // elem}]", off % elem
        if tid and self.type_by_id.get(tid, {}).get("kind") in ("struct", "union"):
            self.type_has_pointer(tid)  # builds self._gen
            try:
                lay = self._gen.layout(tid)
                for m, f in zip(self._gen.members[tid], lay["fields"], strict=True):
                    if (f["offset"] or 0) == rem * 8 and not m.get("bits") and m["id"] in self.fields:
                        target = f"&{prefix}.{self.fields[m['id']]['final_name']}"
                        break
            except KeyError:
                pass
            if target is None and not self.type_has_pointer(tid):
                target = f"(u_char*)&{prefix} + {rem}"
        elif not tid and off == 0:
            target = f"&{bname}"
        elif not tid or not self.type_has_pointer(tid):
            target = f"(u_char*)&{bname} + {off}"
        if target is None:
            self.report["view-unresolved"].append(f"{r['id']} {r['final_name']} = {base_id}+{off}")
            return None
        return f"(({vtype}*)({target}))" if arr else f"(*({vtype}*)({target}))"

    def copy_globals(self) -> None:
        self.plan_views()
        deferred = []
        for r in self.data:
            if note(r, "dup-of") or r["linkage"] == "local-static":
                continue
            loc = r["final_location"].strip()
            name = c_name(r["final_name"])
            if self.take_owned(r["id"], name):
                continue
            if r["id"] == SCRATCH_GLOBAL:
                tname = c_name(self.type_by_id[self.canon_type(r["type_id"])]["final_name"])
                hdr = sibling_header(loc)
                self.out(hdr).items.append(Item("macro", f"// decomp: {r['name']} ({r['key']}): the start of scratch "
                                                f"memory\n#define {name} (*({tname}*)Core_ScratchBase())\n",
                                                name, r["id"], [r["type_id"]], (0,)))
                self.macro_body[name] = f"(*({tname}*)Core_ScratchBase())"
                self.provide(name, hdr)
                continue
            ov = r["overlay"] or (r["instances"].split() or ["SLUS_010.40"])[0]
            d = self.pick_decl(r["id"], kind="var", definition=True)
            e = None
            prov = f"// decomp: {r['name']} ({r['key']})"
            if d is not None:
                text = self.name_inline_type(
                    self.drop_storage(self.own_declarator(d, self.renamed(d["file"], d["start"], d["end"], ov))).strip(), d)
                text = self.apply_fixups(r, text)
                decl, init = self.split_init(text)
                original = self.src.load(d["file"])[d["start"]:d["end"]].decode("utf-8")
                string_include = re.search(r'#\s*include\s+"build/assets/([^"]+)\.vsString"', original)
                if not re.search(rf"\b{re.escape(name)}\b", decl):
                    # defined under a template binding macro (`static CVECTOR VS_CLOSED_RING_COLORS[] = ...`)
                    decl = re.sub(r"\b(\w+)(\s*(?:\[[^\]]*\])*)\s*$",
                                  lambda match, symbol_name=name: f"{symbol_name}{match[2]}", decl, count=1)
                    text = decl + (f" = {init}" if init is not None else "")
                if string_include is not None:
                    asset = Path(string_include[1])
                    module = asset.parent.as_posix()
                    extent = self.string_asset_extents.get((module, asset.name))
                    if extent is None or extent[1] <= 0:
                        raise ValueError(f"{r['id']}: unresolved disc string asset {asset}")
                    if not re.match(r"u_short\s+", decl):
                        raise ValueError(f"{r['id']}: unexpected disc string declaration {decl}")
                    decl = re.sub(rf"\b{re.escape(name)}\s*\[[^\]]*\]",
                                  f"{name}[{(extent[1] + 1) // 2}]", decl, count=1)
                    text = f"{decl}; /* contents: disc string table (ModuleData.c) */"
                    self.string_asset_sources[r["id"]] = (module, extent[0])
                    self.module_data.append((r, name, extent[1]))
                elif init is not None and "#include" in init:
                    text = f"{decl} = {{0}}; /* disc data: {r['disposition']}, loaded in Step 5 */"
                    self.report["disc-initializer"].append(r["final_name"])
                else:
                    if r["id"] in self.reset_ids and init is not None:
                        for o in self.reset_ids[r["id"]]:
                            self.resets[(loc, o)].append((name, text))
                    text += ";"
                need = d["need"]
            else:
                e = self.pick_decl(r["id"], kind="var", definition=False) or self.block_extern(r["id"])
                if e is None:
                    self.report["global-no-declaration"].append(f"{r['id']} {r['final_name']} ({r['key']})")
                    continue
                decl = self.name_inline_type(
                    re.sub(r"^\s*extern\s+", "", self.own_declarator(e, self.renamed(e["file"], e["start"], e["end"], ov))).strip(), e)
                decl = self.apply_fixups(r, decl)
                if not re.search(rf"\b{re.escape(name)}\b", decl):
                    # declared in a template header under its binding macro (`extern CVECTOR VS_RING_COLORS[];`)
                    decl = re.sub(r"\b(\w+)(\s*(?:\[[^\]]*\])*)\s*$",
                                  lambda match, symbol_name=name: f"{symbol_name}{match[2]}", decl, count=1)
                if re.search(r"\[\s*\]", decl):
                    size = int(r["size"] or 0)
                    elem = e.get("elem_size") or 0
                    ext = self.asset_extents.get((r["overlay"], r["name"]))
                    if ext is not None:
                        size = ext[1]  # the ledger measures assets to the next symbol, splat to the next subsegment
                    if size > 0 and elem > 0:
                        decl = re.sub(r"\[\s*\]", f"[{size // elem}]", decl, count=1)
                    elif size > 0 and (m := re.match(rf"(.*?)\b{re.escape(name)}\s*\[\s*\](.*)$", decl)):
                        # element size unknown to the index (template-bound declarations): let C compute it
                        decl = f"{m[1]}{name}[{size} / sizeof({m[1].strip()}{m[2]})]{m[2]}"
                    else:
                        self.report["global-unsized"].append(r["final_name"])
                if re.fullmatch(r"void\s+\w+", decl.strip()):
                    # an assembly label declared `extern void` (only its address is used): bytes of its size
                    decl = f"u_char {name}[{max(int(r['size'] or 0), 1)}]"
                tag = {"bss": "", "unknown": ""}.get(r["segment"], f" /* {r['segment']}: contents loaded in Step 5 */")
                if self.module_data_source(r, decl) is not None:
                    tag = " /* contents: module image (ModuleData.c) */"
                    # filled at start-up, so it cannot live in read-only memory (pointer-free: const is top-level)
                    decl = re.sub(r"\bconst\s+", "", decl)
                    # an array the copy could not size: the ledger's byte count stands in for sizeof
                    self.module_data.append((r, name, int(r["size"] or 0) if re.search(r"\[\s*\]", decl) else None))
                elif self.module_data_source(r, pointers=True) is not None and \
                        (count := self.pointer_array_count(decl, name)):
                    tag = " /* contents: module image, relocated (ModulePointers.c) */"
                    # filled at start-up: the elements (pointers) cannot be const
                    decl = re.sub(r"\*\s*const\b\s*", "*", decl)
                    if any(i in self._ptr_typedefs for i in IDENT.findall(decl)):
                        decl = re.sub(r"^\s*const\s+", "", decl)
                    self.module_pointers.append((r, name, count))
                elif tag:
                    self.report["module-data-skipped"].append(
                        f"{r['id']} {r['final_name']} ({r['overlay'] or r['instances']}, pointers={r['has_pointers']})")
                text = f"{decl};{tag}"
                need = e["need"]
            src = (d if d is not None else e)["file"]
            if r["id"] in self.ptr_globals:
                text = self.retype_global(r["id"], name, text)
                decl = self.retype_global("", name, decl) if d is None else decl
            if r["id"] in self.view_base_min:
                n_min = self.view_base_min[r["id"]]

                def grow(s: str, n_min: int = n_min, name: str = name) -> str:
                    return re.sub(rf"(\b{re.escape(name)}\s*)\[(\d+)\]", lambda m: f"{m[1]}[{max(int(m[2]), n_min)}]",
                                  s, count=1)
                text, decl = grow(text), grow(decl)
                self.report["view-base-grown"].append(f"{r['final_name']} -> {n_min}")
            if r["id"] in self.views:
                deferred.append((r, name, decl, text, prov, loc, need, src, d, e))
                continue
            self.emit_global(r, name, decl, text, prov, loc, need, src, d, e)
        for r, name, decl, text, prov, loc, need, src, d, e in deferred:
            macro = self.view_macro(r, name, decl)
            if macro is None:
                self.emit_global(r, name, decl, text, prov, loc, need, src, d, e)
                continue
            hdr = sibling_header(loc)
            self.out(hdr).items.append(Item("macro", f"{prov}: a view into another global\n#define {name} {macro}\n",
                                            name, r["id"], need, (0,), src))
            self.macro_body[name] = macro
            self.provide(name, hdr)
            self.emitted[r["id"]] = d if d is not None else e
            self.report["view-macro"].append(f"{r['final_name']} = {macro}")

    def emit_global(self, r: dict, name: str, decl: str, text: str, prov: str, loc: str, need: list, src: str,
                    d: dict | None, e: dict | None) -> None:
        self.out(loc).items.append(Item("var", f"{prov}\n{text}\n", name, r["id"], need, (1, r["id"]), src))
        self.emitted[r["id"]] = d if d is not None else e
        self.item_row[name] = r["id"]
        hdr = sibling_header(loc)
        self.out(hdr).items.append(Item("extern", f"extern {decl if d is None else self.split_init(text)[0]};\n",
                                        name, r["id"], need, (3, r["id"]), src))
        self.provide(name, hdr)

    # ------------------------------------------------------------------------------------------------- types
    def inline_type(self, r: dict) -> str | None:
        """'local' for a type defined inside a function body, 'nested' for one defined inside another type: both are
        written where they stand, as part of the enclosing text."""
        self.load_index_ranges()
        base = rx_norm(r["key"].split("#")[0])
        for k, v in self._ix["types"].items():
            if rx_norm(k.split("@")[0]) != base:
                continue
            if any(a <= v["line"] <= b for a, b in self._fn_lines[v["file"]]):
                return "local"
            if any(a < v["line"] <= b and (a, b) != (v["line"], v["end_line"]) for a, b in self._type_lines[v["file"]]):
                return "nested"
        return None

    def merged_layout_types(self) -> set[str]:
        with (LEDGERS / "UPSTREAM_LAYOUTS.csv").open(encoding="utf-8", newline="") as fh:
            return {r["type_id"] for r in csv.DictReader(fh) if r["change"] == "layout"}

    def spelled_type(self, spelling: str) -> str:
        """A member type spelled with decomp type names, in final names."""
        if not hasattr(self, "_tname"):
            self._tname, self._tid_by_name = {}, {}
            for r in self.types:
                for n in [r["name"], *r["aliases"].split()]:
                    if "#" not in r["key"] or n not in self._tname:
                        self._tname.setdefault(n, c_name(self.type_by_id[self.canon_type(r["id"])]["final_name"]))
                        self._tid_by_name.setdefault(n, self.canon_type(r["id"]))
        return re.sub(r"\b(?!struct\b|union\b|enum\b)[A-Za-z_]\w*\b", lambda m: self._tname.get(m[0], m[0]), spelling)

    def generated_type(self, r: dict, name: str) -> str:
        """Struct written from the ledger layout: upstream-merged layouts, whose pinned text has other members."""
        sys.path.insert(0, str(REPO / "tools" / "naming" / "sym_gen"))
        from common import Gen
        if not hasattr(self, "_gen"):
            self._gen = Gen("Shared")
        g = self._gen
        lay = g.layout(r["id"])
        kw = "union" if r["kind"] == "union" else "struct"
        lines = [f"typedef {kw} {name} {{"]
        sizes = {"char": 1, "short": 2, "int": 4, "long": 4}

        def scalar_size(s: str) -> int:
            return next((v for k, v in sizes.items() if re.search(rf"\b{k}\b", s)), 0)

        fields_all = lay["fields"]
        for idx, (m, f) in enumerate(zip(g.members[r["id"]], fields_all, strict=True)):
            t = self.spelled_type(f["type"]).strip()
            ft = (self.fields.get(m["id"]) or {}).get("type", "").strip()
            nxt = next((x["offset"] for x in fields_all[idx + 1:] if x["offset"] > f["offset"]), None)
            gap = (nxt - f["offset"]) // 8 if nxt is not None else 4
            if ft and ft != f["type"] and re.fullmatch(self.SCALAR, t) and re.fullmatch(self.SCALAR, ft) and \
                    not m.get("bits") and scalar_size(ft) <= gap:
                # a merged upstream layout: the field ledger has the member's real width (Field::GimmickInstance
                # unk28 is a u_short next to unk2A, where the pinned view had one int)
                self.report["layout-type-from-ledger"].append(f"{m['id']} {t} -> {ft}")
                t = ft
            if m["id"] in self.ptr_fields and re.fullmatch(self.BASE_INT, t):
                t = "uintptr_t"
                self.report["hazard-member-uintptr"].append(m["id"])
            elif m["id"] in cf.ONDISC_OFFSETS:
                t = "u_int"  # offset from the struct (on-disc pointer)
                self.report["ondisc-member"].append(m["id"])
            elif t == "size_t":
                t = "u_int"
            fname = self.fields[m["id"]]["final_name"]
            if "(*)" in t:
                decl = t.replace("(*)", f"(*{fname})", 1)
            else:
                arr = re.search(r"(\[[^\]]*\])+$", t)
                base, dims = (t[:arr.start()].strip(), arr[0]) if arr else (t, "")
                decl = f"{base} {fname}{dims}" if not base.endswith("*") else f"{base}{fname}{dims}"
            bits = int(m.get("bits") or 0)
            lines.append(f"    /* 0x{(f['offset'] or 0) // 8:02X} */ {decl}{f' : {bits}' if bits else ''};")
        lines.append(f"}} {name};")
        return "\n".join(lines)

    SDK_POINTER_TYPES = {"TIM_IMAGE", "DIRENTRY"}

    def type_has_pointer(self, tid: str, seen: frozenset = frozenset()) -> bool:
        """Whether a ledger type holds a pointer (directly, in a nested type or as a uintptr_t member): its x64 size
        differs from the PS1 one by design."""
        tid = self.canon_type(tid)
        if tid in seen or tid in cf.POINTER_FREE_TYPES:
            return False
        self.spelled_type("")  # builds the decomp-name -> type id table
        sys.path.insert(0, str(REPO / "tools" / "naming" / "sym_gen"))
        from common import Gen
        if not hasattr(self, "_gen"):
            self._gen = Gen("Shared")
        try:
            fields = self._gen.layout(tid)["fields"]
        except KeyError:
            return True  # unknown layout: make no claim
        if any(m["id"] in self.ptr_fields for m in self._gen.members.get(tid, [])):
            return True
        for m, f in zip(self._gen.members.get(tid, []), fields, strict=True):
            if m["id"] in cf.ONDISC_OFFSETS:
                continue  # a 32-bit offset natively
            t = f.get("type") or ""
            if "*" in t or "(" in t or any(n in self.SDK_POINTER_TYPES for n in IDENT.findall(t)):
                return True
            for n in IDENT.findall(t):
                sub = self._tid_by_name.get(n)
                if sub and self.type_by_id[sub]["kind"] in ("struct", "union") and \
                        self.type_has_pointer(sub, seen | {tid}):
                    return True
        return False

    def size_assert(self, tid: str, name: str) -> str:
        r = self.type_by_id[tid]
        size = int(r["size"] or 0)
        if r["kind"] not in ("struct", "union") or size <= 0 or self.type_has_pointer(tid) or nv.pointer_slots(tid):
            return ""
        self.report["size-assert"].append(name)
        return f'\n_Static_assert(sizeof({name}) == 0x{size:X}, "{name} must keep its PS1 layout");'

    def bitfield_units(self, tid: str, text: str) -> str:
        """MSVC starts a run of bitfields on a boundary of their declared type; GCC on the PS1 packed them into the
        next free bits. A run the PS1 placed off that boundary (Actors::CharacterInit: two u_int bitfields at bit
        24, after a char) is given the smallest storage type that starts on its PS1 offset and holds the run, so
        both layouts agree (tools/port/layout_check.py verifies every type)."""
        try:
            self.type_has_pointer(tid)  # builds self._gen
            lay, members = self._gen.layout(tid), self._gen.members.get(tid, [])
        except KeyError:
            return text
        sizes = {"char": 1, "u_char": 1, "signed char": 1, "unsigned char": 1, "short": 2, "u_short": 2,
                 "unsigned short": 2, "signed short": 2}

        def declared(fname: str) -> tuple[str, int] | None:
            m = re.search(rf"(?m)^\s*(?:/\*[^\n]*?\*/\s*)?((?:unsigned\s+|signed\s+)?\w+)\s+{re.escape(fname)}\s*:\s*(\d+)\s*;", text)
            return (m[1], int(m[2])) if m else None

        # runs of consecutive bitfields as the emitted text declares them, at the PS1 offsets of the ledger layout
        runs, cur = [], []
        for m, f in zip(members, lay["fields"], strict=True):
            d = declared(self.fields[m["id"]]["final_name"]) if m["id"] in self.fields else None
            if d and f.get("offset") is not None:
                cur.append((self.fields[m["id"]]["final_name"], d[0], d[1], f["offset"]))
            elif cur:
                runs.append(cur)
                cur = []
        if cur:
            runs.append(cur)

        def ms_offsets(start: int, widths: list[int], unit_bits: int) -> list[int]:
            out, unit, pos = [], start, start
            for w in widths:
                if pos - unit + w > unit_bits:
                    unit += unit_bits
                    pos = unit
                out.append(pos)
                pos += w
            return out

        for run in runs:
            names = [r[0] for r in run]
            types = [r[1] for r in run]
            widths = [r[2] for r in run]
            ps1 = [r[3] for r in run]
            start = ps1[0]
            tsizes = {sizes.get(t, 4) for t in types}
            if len(tsizes) == 1 and start % (8 * next(iter(tsizes))) == 0 and \
                    ms_offsets(start, widths, 8 * next(iter(tsizes))) == ps1:
                continue  # MSVC places this run where the PS1 did
            for nbytes in (4, 2, 1):
                if start % (8 * nbytes) == 0 and ms_offsets(start, widths, 8 * nbytes) == ps1:
                    break
            else:
                self.report["bitfield-run-unfixable"].append(f"{tid} at bit {start}")
                continue
            for fname, t in zip(names, types, strict=True):
                unsigned = t.startswith(("u_", "unsigned"))
                unit = {1: ("u_char", "char"), 2: ("u_short", "short"), 4: ("u_int", "int")}[nbytes][0 if unsigned else 1]
                text = re.sub(rf"(?m)^(\s*(?:/\*[^\n]*?\*/\s*)?)(?:unsigned\s+|signed\s+)?\w+(\s+{re.escape(fname)}\s*:)", rf"\g<1>{unit}\2",
                              text, count=1)
            self.report["bitfield-run-unit"].append(f"{tid} bit {start}: {nbytes} byte unit")
        return text

    def copy_types(self) -> None:
        merged = self.merged_layout_types()
        for r in self.types:
            tid = r["id"]
            if self.canon_type(tid) != tid:
                continue
            loc = r["final_location"].strip()
            name = c_name(r["final_name"])
            if self.take_owned(tid, name):
                if r["kind"] in ("struct", "union"):
                    self.struct_names.add(name)
                self.type_home[tid] = self.owned[tid]["header"].strip()
                continue
            cands = [d for t in [tid] + [x["id"] for x in self.types if self.canon_type(x["id"]) == tid and x["id"] != tid]
                     for d in self.by_target.get(t, []) if d["kind"] == "type" and d["def"]]
            bodies = [d for d in cands if d["ckind"] in ("STRUCT_DECL", "UNION_DECL", "ENUM_DECL")]
            if tid in merged or (not bodies and r["kind"] in ("struct", "union") and not self.inline_type(r)):
                text = self.generated_type(r, name)
                for old, new, why in cf.TYPE_FIXES.get(tid, []):
                    if old not in text:
                        raise SystemExit(f"copy_fixups: {tid} {r['final_name']}: '{old}' not found ({why})")
                    text = text.replace(old, new)
                if nv.pointer_slots(tid):
                    text = nv.rebase(tid, text, [(m["final_name"], f) for m, f in
                                     zip(self._gen.members[tid], self._gen.layout(tid)["fields"], strict=True)])
                    text = self.bitfield_units(tid, text)
                text += self.size_assert(tid, name)
                need = sorted({self._tid_by_name[n] for f in self._gen.layout(tid)["fields"]
                               if "*" not in f["type"] for n in IDENT.findall(f["type"]) if n in self._tid_by_name})
                self.struct_names.add(name)
                self.out(loc).items.append(Item("type", f"// layout: ledger (upstream merge) for {r['name']}\n{text}\n",
                                                name, tid, need, (2, tid)))
                self.provide(name, loc)
                self.type_home[tid] = loc
                self.type_deps[tid] = need + sorted({self._tid_by_name[n] for f in self._gen.layout(tid)["fields"]
                                                     for n in IDENT.findall(f["type"]) if n in self._tid_by_name})
                continue
            if not bodies and r["kind"] != "typedef":
                why = self.inline_type(r)
                self.report[f"type-{why}" if why else "type-no-definition"].append(f"{tid} {r['final_name']}")
                continue
            if bodies:
                body = sorted(bodies, key=lambda d: (d["file"].startswith("gen/"), d["file"], d["start"]))[0]
                outer = [d for d in self.decls if d["file"] == body["file"] and d["ckind"] == "TYPEDEF_DECL" and
                         d["start"] <= body["start"] and d["end"] >= body["end"]]
                d = outer[0] if outer else body
            else:
                d = sorted([x for x in cands if x["ckind"] == "TYPEDEF_DECL"], key=lambda x: (x["file"], x["start"]))[0]
            ov = overlay_of(d["file"]) if d["file"].startswith("src/") else "SLUS_010.40"
            text = self.renamed(d["file"], d["start"], d["end"], ov).strip()
            text = self.retype_members(tid, text)
            text = self.retype_ondisc(tid, text)
            for old, new, why in cf.TYPE_FIXES.get(tid, []):
                if old not in text:
                    raise SystemExit(f"copy_fixups: {tid} {r['final_name']}: '{old}' not found ({why})")
                text = text.replace(old, new)
            if r["kind"] in ("struct", "union"):
                text = self.bitfield_units(tid, text)
                if nv.pointer_slots(tid):
                    text = nv.rebase(tid, text, [(m["final_name"], f) for m, f in
                                     zip(self._gen.members[tid], self._gen.layout(tid)["fields"], strict=True)])
                # size_t was 32 bits on the PS1; a member of a layout keeps that width (SYSTEM.DAT header offsets)
                text, n = re.subn(r"(?m)^(\s*)size_t(\s+\w+\s*(?:\[[^\]]*\]\s*)*;)", r"\1u_int\2", text)
                if n:
                    self.report["size_t-member-u_int"].append(f"{name} ({n})")
            kw = {"struct": "struct", "union": "union", "enum": "enum"}.get(r["kind"])
            if kw:
                # give anonymous definitions the tag, so pointers to them can be forward-declared
                text = re.sub(rf"^(typedef\s+)?{kw}\s*{{",
                              lambda match, kind=kw, tag=name: f"{match[1] or ''}{kind} {tag} {{", text, count=1)
                if not text.startswith("typedef"):
                    text += f";\ntypedef {kw} {name} {name}"
                if kw != "enum":
                    self.struct_names.add(name)
            text += ";" + self.size_assert(tid, name)
            self.out(loc).items.append(Item("type", f"// decomp: {r['name']} ({d['file']}:{d['line']})\n{text}\n",
                                            name, tid, d["need"], (2, tid), d["file"]))
            self.provide(name, loc)
            self.type_home[tid] = loc
            self.type_deps[tid] = d.get("need", []) + d.get("ptr", [])
            for fid, f in self.fields.items():
                if f["type_id"] == tid and r["kind"] == "enum":
                    self.provide(self.field_name(fid), loc)

    # ------------------------------------------------------------------------------------------------- string ids
    STRING_TABLES = r"Native\Content\include\Content\StringTables.h"

    def load_string_ids(self) -> None:
        """String-table ids from the decomp's generated asset headers (build/assets/**/<table>.h): index and byte
        offset of each string in a disc text table. Tables of the same name exist in several overlays with other
        values, so the native name carries the overlay: VS_menuText_INDEX_title in MENU9 -> Text_Menu9_menuText_Index_title.
        The strings themselves come from the disc (Step 5)."""
        self.string_ids: dict[str, list[tuple[str, str]]] = collections.defaultdict(list)
        self.string_tables: list[tuple[str, str]] = []
        root = dx.GEN / "build" / "assets"
        for p in sorted(root.rglob("*.h")):
            rel = p.relative_to(root).parent.as_posix()          # MENU/MENU9.PRG
            tag = rel.split("/")[-1].split(".")[0].capitalize()  # Menu9
            text = p.read_text(encoding="utf-8")
            out = []
            for em in re.finditer(r"enum\s+(\w+)\s*{(.*?)}", text, re.S):
                lines = []
                for m in re.finditer(r"VS_(\w+?)_(OFFSET|INDEX)_(\w+)\s*=\s*([^,\n}]+)", em[2]):
                    native = f"Text_{tag}_{m[1]}_{m[2].capitalize()}_{m[3]}"
                    self.string_ids[f"VS_{m[1]}_{m[2]}_{m[3]}"].append((rel, native))
                    lines.append(f"    {native} = {m[4].strip()},")
                    self.provide(native, self.STRING_TABLES)
                out.append(f"enum Text_{tag}_{em[1]} {{\n" + "\n".join(lines) + "\n};")
            self.string_tables.append((f"{rel}/{p.stem}", "\n".join(out)))

    def rename_string_ids(self, text: str, overlay: str) -> str:
        def sub(m: re.Match) -> str:
            cands = self.string_ids.get(m[0])
            if not cands:
                return m[0]
            for rel, native in cands:
                if overlay.endswith(rel) or rel.endswith(overlay):
                    return native
            return cands[0][1]
        return re.sub(r"\bVS_\w+?_(?:OFFSET|INDEX)_\w+\b", sub, text)

    def copy_string_tables(self) -> None:
        f = self.out(self.STRING_TABLES)
        body = "\n\n".join(f"// {name}\n{text}" for name, text in self.string_tables)
        f.items.append(Item("macro", "// Generated from the decomp's asset definitions (tools/port/gen_decomp_headers.py):"
                            " string ids per disc text table.\n" + body + "\n", "", "", [], (0,)))

    # ------------------------------------------------------------------------------------------------- templates
    def plugin_includes(self, rel: str) -> list[tuple[int, str, list[str]]]:
        """(line, template stem, active macro keys) for every `#include "src/EFFECT/<stem>.h"` of a plugin file,
        with the macros defined (and not #undef'd) in that file before it."""
        lines = self.src.load(rel).decode("utf-8", errors="replace").splitlines()
        active: dict[str, str] = {}
        out = []
        for i, line in enumerate(lines, 1):
            m = re.match(r"\s*#\s*(define|undef)\s+(\w+)", line)
            if m and m[1] == "define":
                active[m[2]] = f"{rel}:{i}"
            elif m:
                active.pop(m[2], None)
            m = re.match(r'\s*#include\s+"src/EFFECT/(\w+)\.h"', line)
            if m:
                out.append((i, m[1], list(active.values())))
        return out

    def macro_text(self, key: str) -> str:
        m = self.macro_defs[key]
        start = self.src.offset(m["file"], m["line"])
        end = self.src.line_end(m["file"], m["end"])
        return self.renamed(m["file"], start, end, overlay_of(m["file"])).rstrip()

    def copy_templates(self) -> None:
        """Effect templates (plan Step 4 item 8): one shared body per template, each plugin instance a binding.
        Templates\\<Name>.inl holds the template header's own preprocessor setup and the body; Templates\\<Name>.c
        has one block per instance that defines that plugin's bindings (renamed), includes the body and undefines
        them again. The body text is shared, so a fix to a template is made once."""
        bodies = {}
        for r in self.cat:
            if r["kind"] != "template-body":
                continue
            hdr = "src/" + r["file"]
            d = next((x for x in self.decls if x["file"] == hdr and x["kind"] == "function" and x["def"] and
                      x["line"] == int(r["line"])), None)
            if d is None:
                self.report["template-no-body"].append(r["final_name"])
                continue
            text = self.src.load(hdr).decode("utf-8", errors="replace").replace("\r\n", "\n")
            lines = text.split("\n")
            fn_start_line = d["line"]
            # the header's preprocessor setup before the body (template options, helper macros), not its includes
            setup, names, i = [], [], 0
            while i < fn_start_line - 1:
                line = lines[i]
                if re.match(r"\s*#", line) and not re.match(r"\s*#\s*(include|pragma)\b", line):
                    j = i
                    while lines[j].rstrip().endswith("\\") and j + 1 < len(lines):
                        j += 1
                    s, e = self.src.offset(hdr, i + 1), self.src.line_end(hdr, self.src.offset(hdr, j + 1))
                    setup.append(self.renamed(hdr, s, e, "EFFECT"))
                    m = re.match(r"\s*#\s*define\s+(\w+)", line)
                    if m and m[1] not in names:
                        names.append(m[1])
                    i = j
                i += 1
            fn = self.drop_storage(self.renamed(hdr, d["start"], d["end"], "EFFECT"))
            fn = self.apply_fixups(r, fn)
            macro = r["name"]
            stem = r["final_location"].strip()[:-2]
            inl = stem + ".inl"
            self.out(inl).items.append(Item(
                "function", f"// Template body: {r['final_name']} (decomp {hdr}:{r['line']}). Included once per "
                f"instance by {include_spelling(stem + '.h')[:-2]}.c with that plugin's bindings.\n"
                + ("\n".join(setup) + "\n\n" if setup else "") + fn + "\n"
                + "".join(f"#undef {n}\n" for n in names), macro, r["id"], d["need"], (2, r["file"], int(r["line"]))))
            bodies[(r["file"], macro)] = (inl, d, self.head_of(fn))
        includes_by_file: dict[str, list] = {}
        for r in self.cat:
            if not r["kind"].startswith("template:") or note(r, "dup-of"):
                continue
            if self.take_owned(r["id"], c_name(r["final_name"])):
                continue
            rel = "src/" + r["file"]
            if rel not in includes_by_file:
                includes_by_file[rel] = self.plugin_includes(rel)
            macro = r["kind"].rsplit(":", 1)[-1]
            inc = next(((ln, stem, act) for ln, stem, act in includes_by_file[rel]
                        if ln > int(r["line"]) and (f"EFFECT/{stem}.h", macro) in bodies), None)
            body = bodies.get((f"EFFECT/{inc[1]}.h", macro)) if inc else None
            if body is None:
                self.report["template-instance-unbound"].append(f"{r['id']} {r['final_name']}")
                continue
            inl, d, head = body
            name = c_name(r["final_name"])
            defs = [self.macro_text(k) for k in inc[2] if k in self.macro_defs]
            undefs = [re.match(r"\s*#\s*define\s+(\w+)", t)[1] for t in defs]
            block = (f"// decomp: {r['overlay']} {r['name']} (src/{r['file']}:{r['line']}): {macro}, "
                     f"instance of {include_spelling(inl)}\n" + "\n".join(defs) + "\n"
                     f'#include "{include_spelling(inl)}"\n' + "".join(f"#undef {n}\n" for n in undefs))
            block = self.apply_fixups(r, block)
            loc = r["final_location"].strip()
            self.out(loc).items.append(Item("function", block, name, r["id"], d["need"],
                                            (2, r["file"], int(r["line"]))))
            proto = re.sub(rf"\b{macro}\b", name, head, count=1)
            hdr = sibling_header(loc)
            self.out(hdr).items.append(Item("proto", f"{proto};\n", name, r["id"], d["need"],
                                            (4, r["file"], int(r["line"]))))
            self.provide(name, hdr)
            self.item_row[name] = r["id"]
            self.emitted[r["id"]] = d

    # ------------------------------------------------------------------------------------------------- plugins
    EFFPURGE = "src/EFFECT/EFFPURGE.BIN/0.c"

    def load_plugin_slots(self) -> None:
        """Effect plugins all load at one address, so EFFPURGE's entry table names functions only by address
        (refs.json `plugin:func_X`). Effect e owns slots [_effectEntryPointIndices[e], [e+1]) of
        _effectEntryPoints and loads _plgFiles[e]; the dev routine func_800F9800 calls every entry in table order.
        Each reference therefore resolves to the function of that name in the plugin of its slot's effect."""
        self.plugin_ref: dict[int, str] = {}
        b = self.src.load(self.EFFPURGE)
        text = b.decode("latin-1")

        def items(name: str) -> list[tuple[int, str]]:
            m = re.search(rf"\b{name}\[\]\s*=\s*{{", text)
            end = text.index("};", m.end())
            return [(m.end() + x.start(), x[0]) for x in re.finditer(r"PLG\(\d+\)|\w+", text[m.end():end])]

        indices = [int(v, 0) for _, v in items("_effectEntryPointIndices")]
        plg = [(re.match(r"PLG\((\d+)\)", v)[1] if v.startswith("PLG(") else None) for _, v in items("_plgFiles")]
        entries = items("_effectEntryPoints")

        def plugin_of(slot: int) -> str | None:
            # the last effect whose slice starts at or before the slot (empty slices share a start index)
            e = max((i for i, start in enumerate(indices) if start <= slot), default=None)
            return f"PLG{plg[e]}.BIN" if e is not None and e < len(plg) and plg[e] else None

        slots = [(off, name, plugin_of(k)) for k, (off, name) in enumerate(entries)]
        f = re.search(r"\nint func_800F9800\([^)]*\)\s*\{", text)
        calls = [(f.end() + m.start(), m[1]) for m in re.finditer(r"\b(\w+)\(NULL, 2, 0\)",
                                                                  text[f.end():text.index("\n}", f.end())])]
        if [n for _, n in calls] != [n for _, n, _ in slots if n != "NULL"]:
            self.report["plugin-calls-not-in-table-order"].append(self.EFFPURGE)
            calls = []
        table = [(off, name, ov) for off, name, ov in slots if name != "NULL"]
        by_name = {(r["overlay"], r["name"]): r for r in self.cat}
        twins = self.plugin_twins()
        for (off, name, ov), call in zip(table, calls or [None] * len(table), strict=True):
            row = by_name.get((ov, name)) or by_name.get((twins.get(ov), name)) if ov else None
            if row is None and ov and name == "func_800F9800":
                # every plugin's code starts at 0x800F9800: its first function (PLG044: vs_effect_solidShock)
                plg = twins.get(ov, ov)
                firsts = sorted((r for r in self.cat if r["overlay"] == plg and r["kind"] != "template-body"),
                                key=lambda r: (r["file"], int(r["line"] or 0)))
                row = firsts[0] if firsts else None
            if row is None:
                self.report["plugin-entry-unresolved"].append(f"{name} ({ov})")
                continue
            cname = c_name(row["final_name"])
            self.plugin_ref[off] = cname
            if call:
                self.plugin_ref[call[0]] = cname

    @staticmethod
    def plugin_twins() -> dict[str, str]:
        """48 plugin files on the disc have no decomp source because they are byte-identical to one that does
        (plan §4.6): map each to that plugin, comparing the shipped files in SLUS-01040\\EFFECT."""
        import hashlib
        root, covered = REPO / "SLUS-01040" / "EFFECT", {p.name for p in (dx.SRC / "EFFECT").iterdir() if p.is_dir()}
        by_hash: dict[str, list[str]] = collections.defaultdict(list)
        for p in sorted(root.glob("PLG*.BIN")):
            data = p.read_bytes()
            if data:
                by_hash[hashlib.sha1(data).hexdigest()].append(p.name)
        twins = {}
        for names in by_hash.values():
            cov = [n for n in names if n in covered]
            for n in names:
                if n not in covered and cov:
                    twins[n] = cov[0]
        return twins

    # ------------------------------------------------------------------------------------------------- reset on load
    RESET_FILE = r"Native\Game\src\Boot\OverlayReset.c"

    def reset_rows(self) -> dict[str, str]:
        """Statics with an initial value in modules the original loads again (menu screens, title, ending, effect
        plugins): reloading re-read their data from the disc, so their initial values came back each time
        (PORTING_WATCHLIST.md §1). Zero-initialised statics were not cleared and are not reset."""
        out = {}
        for r in self.data:
            ov = r["overlay"] or (r["instances"].split() or [""])[0]
            if r["disposition"] == "state" and r["init"] not in ("", "0") and not note(r, "dup-of") \
                    and ov.startswith(("MENU/", "TITLE/", "ENDING/", "EFFECT/")):
                for o in (r["instances"].split() if r["linkage"] == "per-instance" else [ov]):
                    out.setdefault(r["id"], [])
                    out[r["id"]].append(o.split("/")[-1])
        return out

    @staticmethod
    def overlay_tag(ov: str) -> str:
        stem = ov.split(".")[0]
        return {"MAINMENU": "MainMenu"}.get(stem, stem.capitalize())

    def hoist_local_statics(self, r: dict, text: str, loc: str) -> str:
        """Move this function's reset-on-load statics to file scope, so the module reset can reach them."""
        for gid in self.local_static_ids.get(r["file"], {}).get(r["name"], []):
            name = c_name(self.data_by_id[gid]["final_name"])
            m = re.search(rf"(?m)^[ \t]*static\b[^;{{}}]*?\b{re.escape(name)}\b", text)
            if not m:
                self.report["reset-local-not-found"].append(f"{gid} {name}")
                continue
            depth, i = 0, m.start()
            while i < len(text) and not (text[i] == ";" and depth == 0):
                depth += {"{": 1, "(": 1, "}": -1, ")": -1}.get(text[i], 0)
                i += 1
            stmt = text[m.start():i].strip()
            if len(self.top_level_split(stmt)) > 1:
                self.report["reset-local-multi-declarator"].append(f"{gid} {name}")
                continue
            local = set(IDENT.findall(strip_code(stmt))) & self.function_local_names - {name}
            if local:
                # the initial value names something that exists only inside the function (a local enum constant,
                # another local static): it stays where it is and is not reset
                self.report["reset-local-not-hoisted"].append(f"{gid} {name} ({', '.join(sorted(local))})")
                continue
            decl = re.sub(r"^static\s+", "", stmt)
            if name in self.global_names:
                # a function-local name only has to be unique in its function: qualify it at file scope
                new = f"{name}_{c_name(r['final_name']).rsplit('_', 1)[-1]}"
                text = re.sub(rf"\b{re.escape(name)}\b", new, text)
                decl = re.sub(rf"\b{re.escape(name)}\b", new, decl)
                name = new
            self.global_names.add(name)
            end = i + 1 + (1 if text[i + 1:i + 2] == "\n" else 0)
            text = text[:m.start()] + text[end:]
            self.out(loc).items.append(Item("var", f"// decomp: local static of {r['name']}, at file scope so "
                                            f"the module reset reaches it\nstatic {decl};\n", name, gid, [],
                                            (1, gid), r["file"]))
            for ov in self.reset_ids[gid]:
                self.resets[(loc, ov)].append((name, decl))
        return text

    def copy_resets(self) -> None:
        by_ov: dict[str, list[str]] = collections.defaultdict(list)
        for (loc, ov), entries in sorted(self.resets.items()):
            stem = include_spelling(sibling_header(loc))[:-2].replace("/", "_")
            parts = stem.split("_")
            if len(parts) > 1 and parts[-1] == parts[-2]:
                stem = "_".join(parts[:-1])
            fn = f"{stem}_Reset{self.overlay_tag(ov)}Statics"
            body = []
            for name, decl in entries:
                init_decl = re.sub(rf"\b{re.escape(name)}\b", "initial", decl, count=1)
                body.append(f"    {{\n        static const {init_decl};\n"
                            f"        memcpy((void*)&{name}, &initial, sizeof {name});\n    }}\n")
            self.out(loc).items.append(Item(
                "function", f"// {ov} reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).\n"
                f"void {fn}(void)\n{{\n" + "".join(body) + "}\n", fn, "", [], (3, ov)))
            hdr = sibling_header(loc)
            self.out(hdr).items.append(Item("proto", f"void {fn}(void);\n", fn, "", [], (5, ov)))
            self.provide(fn, hdr)
            by_ov[ov].append(fn)
        for ov, fns in sorted(by_ov.items()):
            fn = f"Boot_Reset{self.overlay_tag(ov)}Statics"
            self.out(self.RESET_FILE).items.append(Item(
                "function", f"// Call where the original loads {ov}: restores the statics its reload re-read from "
                f"the disc.\nvoid {fn}(void)\n{{\n" + "".join(f"    {f}();\n" for f in fns) + "}\n",
                fn, "", [], (2, ov)))
            self.out(sibling_header(self.RESET_FILE)).items.append(Item("proto", f"void {fn}(void);\n", fn, "",
                                                                          [], (4, ov)))
            self.provide(fn, sibling_header(self.RESET_FILE))

    # ------------------------------------------------------------------------------------------------- macros
    DISC_LAYOUT = r"Native\Content\include\Content\DiscLayout.h"
    MODULE_DATA = r"Native\Game\src\Boot\ModuleData.c"

    def module_data_source(self, r: dict, decl: str = "", pointers: bool = False) -> tuple[str, int] | None:
        """Where a global with no C initializer gets its contents: (module file on the disc, byte offset). Only
        pointer-free data with one known home, unless `pointers` (tables the loader relocates)."""
        if r["id"] in self.string_asset_sources:
            return self.string_asset_sources[r["id"]]
        if r["segment"] in ("bss", "") or not r["overlay"] or (r["has_pointers"] != "0" and not pointers):
            return None
        if decl and self.decl_has_pointer(decl, c_name(r["final_name"])):
            return None
        ext = self.asset_extents.get((r["overlay"], r["name"]))
        if ext is not None:
            return r["overlay"], ext[0]
        if r["file_offset"].strip():
            return r["overlay"], int(r["file_offset"], 0)
        return None

    def decl_has_pointer(self, decl: str, name: str) -> bool:
        """Whether a native declaration's type holds a pointer: written in the declarator, in a struct or union it
        names, or behind one of the decomp's pointer typedefs (the data ledger's flag misses typedefs)."""
        if not hasattr(self, "_ptr_typedefs"):
            names = set()
            for p in dx.SRC.rglob("*.[ch]"):
                t = p.read_text(encoding="utf-8", errors="replace")
                names |= set(re.findall(r"typedef[^;{}]*\(\s*\*\s*(\w+)\s*\)", t))
                names |= set(re.findall(r"typedef[^;{}()]*\*\s*(\w+)\s*;", t))
            self._ptr_typedefs = {c_name(r["final_name"]) for r in self.types if r["name"] in names} | names
            self._tid_by_cname = {c_name(r["final_name"]): self.canon_type(r["id"]) for r in self.types}
        text = re.sub(r"sizeof\([^()]*\)", "", re.sub(rf"\b{re.escape(name)}\b", "", decl))
        if "*" in text or "(" in text:
            return True
        for ident in IDENT.findall(text):
            if ident in self._ptr_typedefs or ident in self.SDK_POINTER_TYPES:
                return True
            tid = self._tid_by_cname.get(ident)
            if tid and self.type_by_id[tid]["kind"] in ("struct", "union") and self.type_has_pointer(tid):
                return True
        return False

    def pointer_array_count(self, decl: str, name: str) -> int:
        """Element count of a one-dimensional array of pointers (data, function, or a pointer typedef); 0 otherwise."""
        self.decl_has_pointer(decl, name)  # loads the pointer typedefs
        m = re.search(rf"\b{re.escape(name)}\s*\[\s*(\d+)\s*\]\s*(\)|$)", decl)
        if not m:
            return 0
        before = decl[:m.start()]
        if re.search(r"\(\s*\*\s*$", before) or before.rstrip().endswith("*") or \
                any(i in self._ptr_typedefs for i in IDENT.findall(before)):
            return int(m[1])
        return 0

    SPLAT_ARTIFACTS = dx.DECOMP / "build" / "splat-artifacts"
    MODULE_POINTERS = r"Native\Game\src\Boot\ModulePointers.c"

    @staticmethod
    def module_blob_declaration(name: str, size: int) -> tuple[str, int]:
        storage, width = cf.MODULE_DATA_BLOB_TYPES.get(name, ("unsigned int", 4))
        if size <= 0 or size % width != 0:
            raise ValueError(f"Invalid source-data extent for {name}: {size}")
        return f"{storage} {name}[{size // width}]", width

    def copy_module_pointers(self) -> None:
        """Pointer tables kept in the module images (handler tables, string and data pointer arrays): the loader
        reads their PS1 addresses from the user's disc and resolves each through the map of the module's symbols
        (address -> native function or global) from the decomp's splat symbol listing."""
        tables, modules = [], set()
        for r, name, count in self.module_pointers:
            if int(r["size"] or 0) < count * 4:
                self.report["module-pointers-size"].append(f"{r['id']} {r['final_name']} ({r['size']} < {count}*4)")
                continue
            module, offset = self.module_data_source(r, pointers=True)
            sample = cf.MODULE_SAMPLE_POINTERS.get(r["id"])
            owner = f"&{sample}Source" if sample else "NULL"
            tables.append(f'    {{"{module}", 0x{offset:X}, {count}u, (void**){name}, '
                          f'sizeof {name} / sizeof {name}[0], {owner}}}, // {r["id"]} {r["name"]}')
            modules.add(module)
        if modules:
            modules.add("SLUS_010.40")  # the resident module, reachable from every overlay
        emitted_fns = {i.name for f in self.files.values() for i in f.items if i.kind == "function"}
        emitted_fns |= {o["name"].strip() for o in self.owned.values() if o["kind"].strip() == "function"}
        fn_by_key = {(r["overlay"], r["name"]): r for r in self.cat_by_id.values()}
        fn_overlay = lambda m: m.split("/")[-1]  # noqa: E731  (catalog: "BATTLE.PRG"; ledger/splat: "BATTLE/BATTLE.PRG")
        data_by_key = {(r["overlay"], r["name"]): r for r in self.data}
        pointer_arrays = {name: count for _, name, count in self.module_pointers}
        incomplete_arrays = {item.name for output in self.files.values() for item in output.items
                     if item.kind == "extern" and re.search(r"\[\s*\]", item.text)}
        symbols = []
        for module in sorted(modules):
            path = self.SPLAT_ARTIFACTS / module / "splat_symbols.csv"
            if not path.exists():
                self.report["module-pointers-no-symbols"].append(module)
                continue
            seen = set()
            with path.open(encoding="utf-8", newline="") as fh:
                for s in csv.DictReader(fh):
                    if s["defined"] != "True" or s["name"] in seen:
                        continue
                    addr = int(s["vram_start"], 16)
                    if s["type"] == "func":
                        r = fn_by_key.get((fn_overlay(module), s["name"]))
                        hops = 0
                        while r is not None and c_name(r["final_name"]) not in emitted_fns and hops < 8:
                            dup = note(r, "dup-of")
                            r, hops = (self.cat_by_id.get(dup) if dup else None), hops + 1
                        if r is None or c_name(r["final_name"]) not in emitted_fns:
                            continue
                        cname, size = c_name(r["final_name"]), 0
                        layout = "0u, 0u, 0u"
                    else:
                        r = data_by_key.get((module, s["name"]))
                        if r is None or r["id"] not in self.emitted or r["id"] == SCRATCH_GLOBAL or \
                                r["id"] in self.views:
                            continue
                        cname = c_name(r["final_name"])
                        size = max(int(s["size"] or 0), int(r["size"] or 0))  # splat often reports a default 4
                        tid = self.canon_type(r["type_id"]) if r["type_id"] else ""
                        if cname in incomplete_arrays:
                            layout = "0u, 0u, 0u"
                        elif cname in pointer_arrays:
                            size = pointer_arrays[cname] * 4
                            layout = f"sizeof {cname}, 4u, sizeof {cname}[0]"
                        elif r["has_pointers"] == "0" and (not tid or not self.type_has_pointer(tid)):
                            layout = f"sizeof {cname}, 1u, 1u"
                        else:
                            layout = f"sizeof {cname}, 0u, 0u"
                    if cname not in self.provider:
                        continue
                    seen.add(s["name"])
                    symbols.append(f'    {{"{module}", 0x{addr:08X}u, {size}u, (void*)&{cname}, {layout}}},')
        for module, address, fid in cf.MODULE_FUNCTION_ENTRIES:
            name = c_name(self.cat_by_id[fid]["final_name"])
            if name not in emitted_fns or name not in self.provider:
                raise ValueError(f"Missing reviewed module entry {fid}: {name}")
            symbols.append(f'    {{"{module}", 0x{address:08X}u, 0u, (void*)&{name}, 0u, 0u, 0u}},')
        declarations, sample_sources = [], []
        for module, _, address, size, name, _ in cf.MODULE_DATA_BLOBS:
            declaration, width = self.module_blob_declaration(name, size)
            declarations.append(f"extern {declaration};")
            if name in cf.MODULE_SAMPLE_POINTERS.values():
                sample_sources.append(
                    f'static const Content_ModuleSymbol {name}Source = '
                    f'{{"{module}", 0x{address:08X}u, {size}u, (void*){name}, '
                    f'sizeof {name}, {width}u, {width}u}};')
            else:
                symbols.append(f'    {{"{module}", 0x{address:08X}u, {size}u, (void*){name}, '
                       f'sizeof {name}, 1u, 1u}},')
        text = ("// Pointer tables in the module images and the symbol map that relocates them (splat_symbols.csv).\n"
            '#include "Content/ModuleData.h"\n\n' + "\n".join(declarations) + "\n\n"
                + "\n".join(sample_sources) + "\n\n"
                "const Content_ModuleSymbol Content_moduleSymbols[] = {\n" + "\n".join(symbols) + "\n};\n"
                "const unsigned Content_moduleSymbolCount = sizeof Content_moduleSymbols / sizeof Content_moduleSymbols[0];\n\n"
                "const Content_ModulePointerTable Content_modulePointers[] = {\n" + "\n".join(tables) + "\n};\n"
                "const unsigned Content_modulePointerCount = sizeof Content_modulePointers / sizeof Content_modulePointers[0];\n")
        self.out(self.MODULE_POINTERS).items.append(Item("var", text, "Content_moduleSymbols", "", [], (0,)))
        self.report["module-pointer-tables"] = [t.split("// ")[-1] for t in tables]

    def copy_module_data(self) -> None:
        """The table Content_LoadModuleData fills globals from: contents of initialised data the decomp keeps in the
        module images (data, rodata, assets) rather than in C. Positions only; the bytes come from the user's disc."""
        rows = []
        for r, name, bytes_ in sorted(self.module_data, key=lambda x: (x[0]["overlay"], self.module_data_source(x[0])[1])):
            if r["id"] in self.views and name in self.macro_body:
                continue  # a view: its base is filled from the same bytes
            module, offset = self.module_data_source(r)
            if bytes_ is not None and bytes_ <= 0:
                self.report["module-data-unsized"].append(f"{r['id']} {r['final_name']}")
                continue
            size = f"sizeof {name}" if bytes_ is None else f"{bytes_}u"
            rows.append(f'    {{"{module}", 0x{offset:X}, {size}, &{name}, sizeof {name}}}, // {r["id"]} {r["name"]}')
        definitions = []
        for module, offset, _, size, name, evidence in cf.MODULE_DATA_BLOBS:
            declaration, _ = self.module_blob_declaration(name, size)
            definitions.append(f"// decomp: {module} {evidence}\n{declaration};")
            rows.append(f'    {{"{module}", 0x{offset:X}, {size}u, {name}, sizeof {name}}},')
        text = ("// Globals whose contents are in the module images (DATA_LEDGER segment data/rodata/asset, no pointers).\n"
            '#include "Content/ModuleData.h"\n\n' + "\n".join(definitions) + "\n\n"
                "const Content_ModuleDataEntry Content_moduleData[] = {\n" + "\n".join(rows) + "\n};\n"
                "const unsigned Content_moduleDataCount = sizeof Content_moduleData / sizeof Content_moduleData[0];\n")
        self.out(self.MODULE_DATA).items.append(Item("var", text, "Content_moduleData", "", [], (0,)))

    def copy_disc_layout(self) -> None:
        """Disc file positions (VS_<FILE>_LBA / _SIZE). The code does arithmetic on them (sector offset tables,
        token-pasted names), so Phase 1a keeps the real values and Content maps a sector to its converted file at
        run time; replacing them with file ids is modernization. Names, sectors and sizes only: no disc contents."""
        src = dx.GEN / "build" / "src" / "include" / "lbas.h"
        lines = [ln for ln in src.read_text(encoding="utf-8").splitlines() if re.match(r"#define VS_\w+_(LBA|SIZE) ", ln)]
        f = self.out(self.DISC_LAYOUT)
        f.items.append(Item("macro", "// Generated from the disc listing (tools/port/gen_decomp_headers.py).\n" +
                            "\n".join(lines) + "\n", "", "", [], (0,)))
        for ln in lines:
            self.provide(ln.split()[1], self.DISC_LAYOUT)

    def inside_function(self, file: str, line: int) -> bool:
        self.load_index_ranges()
        return any(a < line < b for a, b in self._fn_lines[file])

    def load_index_ranges(self) -> None:
        if hasattr(self, "_ix"):
            return
        self._ix = json.loads((PORT / "index.json").read_text(encoding="utf-8"))
        self._fn_lines = collections.defaultdict(list)
        for f in self._ix["functions"].values():
            self._fn_lines[f["file"]].append((f["line"], f["end_line"]))
        self._type_lines = collections.defaultdict(list)
        for v in self._ix["types"].values():
            self._type_lines[v["file"]].append((v["line"], v["end_line"]))

    def copy_macros(self) -> None:
        defs: dict[str, dict] = {}
        template_headers = {"src/" + r["file"] for r in self.cat if r["kind"] == "template-body"}
        for _key, m in sorted(self.macro_defs.items(), key=lambda kv: (kv[1]["file"].endswith(".c"), kv[0])):
            if dx.origin(m["file"]) != "game" or re.match(r"src/EFFECT/PLG\d+\.BIN/", m["file"]):
                continue  # effect plugin .c files bind template parameters: copied with the templates
            if m["file"] in template_headers and m["name"].startswith("VS_"):
                continue  # a template's parameters and setup: part of its .inl
            if m["name"] in TOKEN_RENAMES:
                continue  # GTE assembly macros: replaced by Core/Gte.h calls
            if self.inside_function(m["file"], m["line"]):
                continue  # defined (and #undef'd) inside a function body: copied with it
            defs.setdefault(m["name"], m)
        users: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
        texts = {}
        for path, f in self.files.items():
            if path.endswith(".inl"):
                continue  # template bodies carry their header's setup macros
            idents = set(IDENT.findall(strip_code("".join(i.text for i in f.items))))
            for n in idents & defs.keys():
                users[n][path] += 1
        pending = list(users)
        while pending:
            n = pending.pop()
            if n in texts:
                continue
            m = defs[n]
            start = self.src.offset(m["file"], m["line"])
            end = self.src.line_end(m["file"], m["end"])
            ov = overlay_of(m["file"]) if m["file"].startswith("src/") else "SLUS_010.40"
            texts[n] = cf.MACRO_REPLACEMENTS.get(n) or self.renamed(m["file"], start, end, ov).rstrip()
            for dep in set(IDENT.findall(strip_code(texts[n]))) & defs.keys():
                if dep != n:
                    for p, c in users[n].items():
                        users[dep][p] += c
                    pending.append(dep)
        local: dict[str, tuple] = {}
        for n, text in texts.items():
            self.macro_body[n] = text
            m = defs[n]
            item = lambda m=m, n=n, text=text: Item(  # noqa: E731
                "macro", f"// decomp: {m['file']}:{m['line']}\n{text}\n", n, "", [], (1, m["file"], m["line"]))
            if m["file"].endswith(".c"):
                local[n] = (m, item)
                continue
            # home: the header used by most users
            homes = collections.Counter()
            for p, c in users[n].items():
                homes[sibling_header(p)] += c
            home = sorted(homes.items(), key=lambda kv: (-kv[1], kv[0]))[0][0]
            self.out(home).items.append(item())
            self.provide(n, home)
        # a .c file's own definition of a name that a header also defines (MENU's mkCdFile takes the VS_ prefix)
        file_defs: dict[tuple[str, str], dict] = {}
        for _key, m in self.macro_defs.items():
            if m["file"].endswith(".c") and m["name"] in texts and not defs[m["name"]]["file"].endswith(".c") \
                    and dx.origin(m["file"]) == "game" and not self.inside_function(m["file"], m["line"]):
                file_defs[(m["file"], m["name"])] = m
        self.wrap_local_macros(local, file_defs)

    def wrap_local_macros(self, local: dict[str, tuple], file_defs: dict[tuple[str, str], dict]) -> None:
        """A decomp .c file's own helper macros (some have generic names such as `offset`) are defined just before
        each copied item of that file that uses them, and removed after it; a file's own definition of a name that a
        header also defines is pushed over the header's (#pragma push_macro) and popped after. Types that use a
        .c-only macro get it in their header."""
        def text_of(m: dict) -> str:
            s, e = self.src.offset(m["file"], m["line"]), self.src.line_end(m["file"], m["end"])
            return self.renamed(m["file"], s, e, overlay_of(m["file"])).rstrip()

        for path, f in self.files.items():
            for i in f.items:
                if i.kind not in ("function", "type", "var", "extern"):
                    continue
                used = set(IDENT.findall(strip_code(i.text)))
                for (file, n), m in file_defs.items():
                    if n in used and file == i.src and (i.kind != "function" or m["line"] < i.order[2]):
                        i.text = (f'#pragma push_macro("{n}")\n#undef {n}\n// decomp: {file}:{m["line"]} '
                                  f"(this file's own definition)\n{text_of(m)}\n{i.text}"
                                  f'#pragma pop_macro("{n}")\n')
                used &= local.keys()
                stack, seen = list(used), set()
                while stack:
                    n = stack.pop()
                    if n in seen:
                        continue
                    seen.add(n)
                    stack += list(set(IDENT.findall(strip_code(self.macro_body[n]))) & local.keys())
                if i.kind == "function":
                    seen = {n for n in seen if local[n][0]["file"] == i.src and local[n][0]["line"] < i.order[2]}
                if not seen:
                    continue
                defs = sorted(seen, key=lambda n: local[n][0]["line"])
                head = "".join(local[n][1]().text for n in defs)
                if i.kind in ("function", "var"):
                    i.text = head + i.text + "".join(f"#undef {n}\n" for n in reversed(defs))
                else:
                    i.text = head + i.text
                    for n in defs:
                        self.provide(n, path)

    # ------------------------------------------------------------------------------------------------- includes
    def ai_frame_references(self, text: str) -> str:
        text = text.replace("*(int*)((uintptr_t)Core_ScratchBase() + 0x3F4)",
                            "(Battle_Ai_FrameState()->roomData != NULL)")

        def replace(match: re.Match) -> str:
            member = cf.AI_FRAME_FIELDS.get(int(match[1], 16))
            if member is None:
                return match[0]
            return f"((uintptr_t)&Battle_Ai_FrameState()->{member})"

        return re.sub(r"\(\(uintptr_t\)Core_ScratchBase\(\)\s*\+\s*0x([0-9A-Fa-f]+)\)", replace, text)

    def finish(self) -> dict[str, str]:
        rendered = {}
        type_by_cname = {c_name(self.type_by_id[t]["final_name"]): t for t in self.type_home}
        for path, f in sorted(self.files.items()):
            if path.startswith("Native\\Game\\src\\Battle\\Ai\\"):
                for item in f.items:
                    item.text = self.ai_frame_references(item.text)
            own = {i.name for i in f.items}
            if not f.is_header:
                includes = self.providers_of(f.items, path, own, None, type_by_cname)[0]
                if path.endswith(".c") and self.norm_header(sibling_header(path)) in self.files:
                    includes.add(self.norm_header(sibling_header(path)))
                lines = ["// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.\n",
                         f'#include "{LEGACY_HEADER}"\n']
                lines += [f'#include "{h}"\n' for h in sorted(include_spelling(x) for x in includes - {path})]
                lines.append("\n")
                lines += [i.text + "\n" for i in self.ordered(f)]
                rendered[path] = "".join(lines)
                continue
            # Headers have a types section (macros, types) and a declarations section (externs, prototypes). Type
            # dependencies are included in types-only mode, so a declaration section always comes after every type
            # it uses and include cycles between declaration sections resolve.
            type_items = [i for i in f.items if i.kind in ("macro", "type")]
            decl_items = [i for i in f.items if i.kind in ("extern", "proto")]
            needed = {self.canon_type(t) for i in type_items for t in i.need}
            t_inc, fwd = self.providers_of(type_items, path, own, needed, type_by_cname)
            self.type_edges[path] = t_inc - {path}
            d_inc, fwd2 = self.providers_of(decl_items, path, own, None, type_by_cname)
            own_structs = {i.name for i in type_items if i.kind == "type"} & self.struct_names
            guard = "VS_" + re.sub(r"\W", "_", include_spelling(path)).upper()
            lines = ["// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.\n",
                     f"#ifndef {guard}_TYPES\n#define {guard}_TYPES\n", f'#include "{LEGACY_HEADER}"\n\n']
            for n in sorted(fwd | fwd2 | own_structs):
                kw = "union" if self.type_by_id[type_by_cname[n]]["kind"] == "union" else "struct"
                lines.append(f"typedef {kw} {n} {n};\n")
            if t_inc - {path}:
                lines.append(f"\n#ifndef VS_TYPES_ONLY\n#define VS_TYPES_ONLY\n#define {guard}_SET_TYPES_ONLY\n#endif\n")
                lines += [f'#include "{h}"\n' for h in sorted(include_spelling(x) for x in t_inc - {path})]
                lines.append(f"#ifdef {guard}_SET_TYPES_ONLY\n#undef VS_TYPES_ONLY\n#undef {guard}_SET_TYPES_ONLY\n"
                             "#endif\n")
            lines.append("\n")
            lines += [i.text + "\n" for i in self.ordered(f) if i.kind in ("macro", "type")]
            lines.append(f"#endif // {guard}_TYPES\n")
            if decl_items:
                lines.append(f"\n#if !defined(VS_TYPES_ONLY) && !defined({guard}_DECLS)\n#define {guard}_DECLS\n")
                lines += [f'#include "{h}"\n' for h in sorted(include_spelling(x) for x in d_inc - {path})]
                lines.append("\n")
                lines += [i.text for i in self.ordered(f) if i.kind in ("extern", "proto")]
                lines.append(f"\n#endif // {guard}_DECLS\n")
            rendered[path] = "".join(lines)
        return rendered

    def providers_of(self, items: list[Item], path: str, own: set[str], needed: set[str] | None,
                     type_by_cname: dict[str, str]) -> tuple[set[str], set[str]]:
        """Headers to include for the names `items` use, and structs that only need a forward declaration
        (`needed` = types that must be complete; None = include everything, for .c files)."""
        body = "".join(i.text for i in items if i.kind != "macro")
        names = set(IDENT.findall(strip_code(body))) - own
        # a macro's body is only compiled where the macro is expanded: users include what it needs
        stack = [n for n in names if n in self.macro_body]
        seen = set()
        while stack:
            m = stack.pop()
            if m in seen:
                continue
            seen.add(m)
            for dep in set(IDENT.findall(strip_code(self.macro_body[m]))) - {m}:
                names.add(dep)
                if dep in self.macro_body:
                    stack.append(dep)
        names -= own
        includes, forwards = set(), set()
        for n in sorted(names):
            h = self.provider.get(n) or cf.NATIVE_HELPER_HEADERS.get(n)
            if h is None or h == path:
                continue
            if needed is not None and n in self.struct_names and type_by_cname.get(n) not in needed:
                forwards.add(n)
                continue
            includes.add(h)
        if needed is None:
            # a .c file can index or dereference a pointer member whose type it never names: include the types
            # reachable from what it uses (two levels)
            ids = {self.item_row.get(n) for n in names} - {None}
            tids = {type_by_cname[n] for n in names if n in type_by_cname}
            for i in ids:
                d = self.emitted.get(i)
                if d:
                    tids |= {self.canon_type(t) for t in d.get("need", []) + d.get("ptr", [])}
            for _ in range(2):
                tids |= {self.canon_type(t) for x in list(tids) for t in self.type_deps.get(x, ())}
            for t in tids:
                r = self.type_by_id.get(t)
                h = self.provider.get(c_name(r["final_name"])) if r else None
                if h and h != path:
                    includes.add(h)
        if any("##_LBA" in self.macro_body.get(m, "") for m in seen) or "##_LBA" in body:
            includes.add(self.DISC_LAYOUT)  # token-pasted disc names
        return includes, forwards

    def report_type_cycles(self) -> None:
        """Headers whose types need each other's types by value cannot be ordered by includes."""
        index, low, stack, on, sccs, n = {}, {}, [], set(), [], [0]

        def visit(v: str) -> None:
            index[v] = low[v] = n[0]
            n[0] += 1
            stack.append(v)
            on.add(v)
            for w in self.type_edges.get(v, ()):
                if w not in index:
                    visit(w)
                    low[v] = min(low[v], low[w])
                elif w in on:
                    low[v] = min(low[v], index[w])
            if low[v] == index[v]:
                comp = []
                while True:
                    w = stack.pop()
                    on.discard(w)
                    comp.append(w)
                    if w == v:
                        break
                if len(comp) > 1:
                    sccs.append(comp)

        sys.setrecursionlimit(10000)
        for v in list(self.type_edges):
            if v not in index:
                visit(v)
        for c in sccs:
            self.report["type-include-cycle"].append(" <-> ".join(include_spelling(p) for p in sorted(c)))

    def ordered(self, f: OutFile) -> list[Item]:
        """Items by section; types in dependency order (a type after every type it holds by value)."""
        types = [i for i in f.items if i.kind == "type"]
        by_row = {i.row: i for i in types}
        done, out = set(), []

        def visit(i: Item, stack: tuple = ()) -> None:
            if i.row in done or i.row in stack:
                return
            for t in i.need:
                dep = by_row.get(self.canon_type(t))
                if dep is not None and dep is not i:
                    visit(dep, stack + (i.row,))
            done.add(i.row)
            out.append(i)

        for i in sorted(types, key=lambda i: i.order):
            visit(i)
        rank = {"macro": 0, "type": 1, "var": 2, "extern": 3, "proto": 4, "function": 5}
        rest = sorted([i for i in f.items if i.kind != "type"], key=lambda i: (rank[i.kind], i.order))
        return [i for i in rest if rank[i.kind] < 1] + out + [i for i in rest if rank[i.kind] > 1]


def mark_ledgers(cp: Copier) -> dict[str, collections.Counter]:
    """Write `copied` and `compiles` (run after the solution has built: plan §4.1 status lifecycle).
    Functions: x = C copied (a template instance or dup-of row counts as its copied code), stub = assembly-only,
    a VS_UNPORTED stub until ported. Globals and types: x when written (function-local statics and nested/local
    types are written inside their function or parent type)."""
    emitted = {}
    for f in cp.files.values():
        for i in f.items:
            if i.row and i.kind in ("function", "var", "type", "macro"):
                emitted[i.row] = "stub" if "not yet ported]" in i.text.split("\n", 1)[0] else "x"
    counts: dict[str, collections.Counter] = {}
    for name, key in (("FUNCTION_CATALOG.csv", "F"), ("DATA_LEDGER.csv", "G"), ("TYPE_LEDGER.csv", "T")):
        path = LEDGERS / name
        with path.open(encoding="utf-8", newline="") as fh:
            rows = list(csv.DictReader(fh))
        cols = list(rows[0].keys())
        c = collections.Counter()
        for r in rows:
            rid = r["id"]
            if key == "F":
                if r["copy"] not in ("yes", "data"):
                    continue
                dup, seen = note(r, "dup-of"), set()
                while dup and dup not in emitted and dup not in seen and dup in cp.cat_by_id:
                    seen.add(dup)
                    dup = note(cp.cat_by_id[dup], "dup-of") or dup
                st = emitted.get(rid) or emitted.get(dup or "") or ("x" if r["kind"] == "template-body" else "")
            elif key == "G":
                st = emitted.get(rid) or ("x" if r["linkage"] == "local-static" or note(r, "dup-of") else "")
            else:
                st = emitted.get(cp.canon_type(rid)) or ("x" if cp.inline_type(r) else "")
            r["copied"] = st
            r["compiles"] = "x" if st else ""
            if rid in cp.owned and r.get("modernized") != "x":
                r["copied"], r["compiles"], r["modernized"] = "x", "x", "wip"
                st = "owned"
            c[st or "missing"] += 1
        with path.open("w", encoding="utf-8", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(rows)
        counts[name] = c
    return counts


def content_hash(data: bytes) -> str:
    """Hash of a file's text with line endings normalised (git may check generated files out with CRLF)."""
    import hashlib
    return hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest()


def write_output(cp: Copier, rendered: dict[str, str]) -> None:
    """Write the generated files, refusing to touch hand work (see the module docstring)."""
    if MANIFEST.exists():
        old = json.loads(MANIFEST.read_text(encoding="utf-8"))
    elif OLD_MANIFEST.exists():
        old = {p: None for p in json.loads(OLD_MANIFEST.read_text(encoding="utf-8"))}  # pre-hash manifest
    else:
        old = {}
    problems = []
    for p, h in sorted(old.items()):
        f = REPO / p
        if h and f.exists() and content_hash(f.read_bytes()) != h:
            problems.append(f"edited by hand since it was generated: {p}")
    for p in sorted(rendered):
        if p not in old and (REPO / p).exists():
            problems.append(f"exists and is not generated (would be overwritten): {p}")
    owned_paths = {o["path"].strip() for o in cp.owned.values()} | {o["header"].strip() for o in cp.owned.values()}
    for p in sorted(owned_paths & set(rendered)):
        problems.append(f"owned file is also a generated file: {p}")
    if problems:
        print("copy_to_native: nothing written. Take ownership with tools\\port\\own.py, or revert the edits:")
        for x in problems:
            print("  " + x)
        raise SystemExit(1)
    for p in old:
        if p not in rendered and (REPO / p).exists():
            (REPO / p).unlink()
    manifest = {}
    for p, text in sorted(rendered.items()):
        dst = REPO / p
        dst.parent.mkdir(parents=True, exist_ok=True)
        data = text.encode("utf-8", errors="surrogateescape")
        if not dst.exists() or dst.read_bytes() != data:
            dst.write_bytes(data)
        manifest[p] = content_hash(data)
    MANIFEST.write_text(json.dumps(manifest, indent=0, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", action="store_true", help="print the full report lists")
    ap.add_argument("--mark", action="store_true", help="after a successful build: fill copied/compiles")
    a = ap.parse_args()
    cp = Copier()
    cp.load_string_ids()
    cp.load_plugin_slots()
    cp.copy_types()
    cp.copy_globals()
    cp.copy_functions()
    cp.copy_templates()
    cp.copy_resets()
    cp.copy_disc_layout()
    cp.copy_module_data()
    cp.copy_module_pointers()
    cp.copy_string_tables()
    cp.copy_macros()
    rendered = cp.finish()
    cp.report_type_cycles()
    write_output(cp, rendered)
    kinds = collections.Counter(i.kind for f in cp.files.values() for i in f.items)
    print(f"{len(rendered)} files ({sum(p.endswith('.c') for p in rendered)} .c), items {dict(kinds)}")
    for k, v in cp.report.items():
        print(f"  {k}: {len(v)}" + ("" if a.report else f"  e.g. {v[:3]}"))
        if a.report:
            for x in v:
                print(f"     {x}")
    if a.mark:
        for name, c in mark_ledgers(cp).items():
            print(f"  {name}: {dict(c)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
