"""Generate the port tracking ledgers from the reference decomp.

Reads resources/rood-reverseWin (read-only) and writes Docs/ledgers/:
  FUNCTION_CATALOG.csv   one row per function (C, INCLUDE_ASM, SDK) with proposed Native system
  SDK_API_LEDGER.md      every PS1 SDK function/macro used by game code -> modern replacement
  SOURCE_FILE_LEDGER.md  every source/header file with hazards and C -> C++ promotion status
  DATA_LEDGER.csv        every data symbol (named + D_) with origin and uses
  SUMMARY.md             roll-up counts

Status columns are left blank on purpose; they are filled in as work proceeds and the
generator preserves them on re-run (matched by key).
"""
from __future__ import annotations

import collections
import csv
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DECOMP = REPO / "resources" / "rood-reverseWin"
SRC = DECOMP / "src"
PSXINC = DECOMP / "include" / "psx"
CONFIG = DECOMP / "config"
OUT = REPO / "Docs" / "ledgers"

STATUS_COLS = ["copied", "compiles", "named", "understood", "modernized", "cpp", "verified", "notes"]

# --------------------------------------------------------------------------- helpers

def read(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="ignore")


def strip_comments(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def unit_of(rel: Path) -> tuple[str, str]:
    parts = rel.parts
    unit = parts[0]
    overlay = parts[1] if len(parts) > 2 else parts[0]
    return unit, overlay


def is_sdk_path(rel: Path) -> str | None:
    parts = [p.lower() for p in rel.parts]
    if parts[0] == "slus_010.40" and len(parts) > 2 and parts[1].startswith("lib"):
        return parts[1]
    if "libpress" in parts:
        return "libpress"
    return None


FUNC_HDR = re.compile(
    r"^(?P<sig>(?:static\s+|inline\s+|extern\s+)*[A-Za-z_][\w \t\*]*?\b(?P<name>[A-Za-z_]\w*)\s*\((?P<params>[^;{}]*?)\))"
    r"\s*(?:__attribute__\s*\(\(.*?\)\)\s*)?\{",
    re.M | re.S,
)
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "do", "else"}
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
INCLUDE_ASM = re.compile(r'INCLUDE_ASM\(\s*"([^"]*)"\s*,\s*(\w+)\s*\)')
GLABEL = re.compile(r"^\s*glabel\s+(\w+)", re.M)

HAZARDS = {
    "hw_reg": re.compile(r"0x1F80(?:[1-9A-F][0-9A-F]{3})\b", re.I),
    "scratchpad": re.compile(r"0x1F800[0-3][0-9A-F]{2}\b|getScratchAddr|\bvs_scratch\b|\bD_1F800\w+", re.I),
    "kseg_literal": re.compile(r"\b0x8[0-9A-F]{7}\b", re.I),
    "ot24": re.compile(r"0x00?FFFFFF\b|<<\s*24\b", re.I),
    "ptr_cast": re.compile(r"\(\s*(?:u_long|long|int|u_int|unsigned int|u32|s32)\s*\)\s*&?\s*[A-Za-z_]\w*(?:->|\.|\[)?", re.I),
    "inline_asm": re.compile(r"__asm__|\basm\s*\(|\bgte_\w+\("),
}


def find_body(text: str, open_idx: int) -> int:
    depth = 0
    i = open_idx
    n = len(text)
    in_str = None
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = None
        elif c in "\"'":
            in_str = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n - 1


# --------------------------------------------------------------------------- SDK surface

def load_sdk() -> dict[str, dict]:
    sdk: dict[str, dict] = {}
    for p in sorted(SRC.rglob("*")):
        if p.suffix not in (".c", ".s"):
            continue
        rel = p.relative_to(SRC)
        lib = is_sdk_path(rel)
        if not lib:
            continue
        t = read(p)
        names = [m.group("name") for m in FUNC_HDR.finditer(strip_comments(t))] if p.suffix == ".c" else GLABEL.findall(t)
        names += [m.group(2) for m in INCLUDE_ASM.finditer(t)]
        for n in names:
            if n in KEYWORDS:
                continue
            sdk.setdefault(n, {"lib": lib, "kind": "function", "defined_in": str(rel)})
    skip = {"asm", "r3000", "gtereg", "gtereg_s", "inline_s", "inline_a", "stdarg", "stddef", "assert"}
    crt = {"abs", "rand", "memory", "limits", "strings", "string", "stdlib", "stdio", "ctype", "setjmp", "qsort",
           "malloc", "convert", "fs"}
    for h in sorted(PSXINC.glob("*.h")):
        stem = h.stem.lower()
        if stem in skip:
            continue
        lib = "crt" if stem in crt else ("libgte" if stem in ("gtemac", "gtenom", "inline_c", "inline_o") else stem)
        t = strip_comments(read(h))
        for m in re.finditer(r"^\s*#\s*define\s+([A-Za-z_]\w*)(\(?)", t, re.M):
            kind = "macro" if m.group(2) else "constant"
            if kind == "constant" and not (stem.startswith("lib") or stem == "kernel"):
                continue
            mlib = "libpad" if re.match(r"^PAD[A-Z]", m.group(1)) else lib
            sdk.setdefault(m.group(1), {"lib": mlib, "kind": kind, "defined_in": f"include/psx/{h.name}"})
        for m in re.finditer(r"^[^#\n]*?\b([A-Za-z_]\w*)\s*\([^;{}()]*(?:\([^()]*\)[^;{}()]*)*\)\s*;", t, re.M):
            name = m.group(1)
            if name not in KEYWORDS:
                sdk.setdefault(name, {"lib": lib, "kind": "function", "defined_in": f"include/psx/{h.name}"})
    for junk in ("NULL", "volatile", "const", "TRUE", "FALSE", "_LIBGPU_H_", "_LIBGTE_H_"):
        sdk.pop(junk, None)
    for name in [n for n, v in sdk.items() if v["kind"] == "constant" and re.match(r"^_\w+_H_?$", n)]:
        sdk.pop(name)
    return sdk


SDK_TARGET = {
    "libgpu": ("Graphics", "VS.Render: PrimitiveList adapter -> RenderQueue (sprites/meshes/quads), Texture/VRAM-page cache, framebuffer readback API"),
    "libgte": ("Core.Math", "VS.Core Math: Vec3/Mat3 fixed-point, Transform/Project, Camera; GTE ops as plain C++ functions"),
    "inline_c": ("Core.Math", "VS.Core Math: replace inline GTE register macros with typed transform calls"),
    "gtemac": ("Core.Math", "VS.Core Math: replace GTE macro sequences with typed transform calls"),
    "libspu": ("Audio", "VS.Audio: Voice/SampleBank/Mixer/Reverb API (no SPU registers or SPU RAM addresses)"),
    "libsnd": ("Audio", "VS.Audio: SequencePlayer / SfxPlayer API"),
    "libcd": ("Content", "VS.Content: VirtualDisc (LBA->file) + AsyncReadQueue"),
    "libds": ("Content", "VS.Content: AsyncReadQueue / StreamReader (St* -> VS.Movie)"),
    "libpress": ("Video", "VS.Movie: StrDecoder (MDEC/VLC/IDCT) -> RGBA frames + XA stream"),
    "libpad": ("Input", "VS.Input: ActionState snapshot per frame, rumble"),
    "libcard": ("Save", "VS.Save: SaveStore (slots), .mcd import/export"),
    "libapi": ("Platform", "VS.Platform: Frame clock, callbacks -> events, file API removed"),
    "libetc": ("Platform", "VS.Platform: FrameClock (VSync), video mode, callbacks"),
    "libc": ("Core", "C++ standard library"),
    "crt": ("Core", "C++ standard library / VS.Core utilities (Random gets a deterministic seeded RNG)"),
    "libsn": ("Platform", "Debug logging (VS.Core Log)"),
    "kernel": ("Platform", "VS.Platform (removed: kernel events/critical sections)"),
}
VIDEO_NAMES = re.compile(r"^(St[A-Z]\w*|DecDCT\w*|MDEC\w*)$")

# Retirement strategy per SDK symbol:
#   R = rewrite call sites directly to the modern API while copying (no shim ever exists)
#   T = temporary shim in Native/<System>/Legacy, translating to the modern API; deleted at 0 uses
#   D = delete the call: no meaning without PS1 hardware (sync, IRQ, kernel events, debug font...)
STRATEGY_BY_LIB = {
    "libgpu": "T", "libgte": "R", "crt": "R", "libc": "R", "libpad": "R", "libetc": "R",
    "libspu": "T", "libsnd": "T", "libds": "T", "libcd": "T", "libpress": "T", "libcard": "T",
    "libapi": "T", "kernel": "D", "libsn": "D",
}
STRATEGY_OVERRIDE = {
    **{n: "D" for n in (
        "EnterCriticalSection", "ExitCriticalSection", "OpenEvent", "CloseEvent", "EnableEvent", "DisableEvent",
        "TestEvent", "UnDeliverEvent", "DeliverEvent", "ResetCallback", "StopCallback", "SetDispMask", "DrawSync",
        "DrawSyncCallback", "InitGeom", "SetGraphDebug", "ResetGraph", "SetDumpFnt", "FntLoad", "FntOpen", "FntFlush",
        "FntPrint", "SystemError", "__main", "_bu_init", "HwCARD", "SwCARD", "SpuQuit", "SpuSetIRQ", "SpuSetIRQAddr",
        "SpuSetIRQCallback", "SpuSetTransferCallback", "SpuSetTransferMode", "SpuSetTransferStartAddr", "SpuInitMalloc",
        "DecDCTReset", "StCdInterrupt", "DsSystemStatus", "DsStartReadySystem", "DsEndReadySystem", "DsReadyCallback",
        "DsFlush", "CdMix", "DsMix", "SetGraphReverse", "SetDrawStp", "_get_mode", "MODE_NTSC")},
    **{n: "R" for n in (
        "VSync", "VSyncCallback", "SetRCnt", "GetRCnt", "StartRCnt", "StopRCnt", "RCntCNT2", "SetVideoMode",
        "getScratchAddr", "rand", "srand", "memcpy", "memset", "memcmp", "strlen", "strcpy", "sprintf", "index",
        "rindex", "abs", "ABS", "exit")},
}


def strategy_of(name: str, lib: str) -> str:
    return STRATEGY_OVERRIDE.get(name, STRATEGY_BY_LIB.get(lib, "T"))

# --------------------------------------------------------------------------- system classification

NAME_RULES = [
    ("Video", r"movie|mdec|dct|\bstr\b|_str[A-Z_]|fmv"),
    ("Audio", r"sound|sfx|music|akao|spu|reverb|voice|volume|bgm|seq(?:uence)?Play|_snd|adsr|wave"),
    ("Input", r"\bpad|_pad|Pad[A-Z]|button|vibrat|rumble|analog"),
    ("Save", r"memcard|card|_save|Save[A-Z]|saveGame|_load(?:Game|Save)|slot"),
    ("Content", r"\bcd|_cd|Cd[A-Z]|lba|queue(?:File|Read)|readFile|loadFile|_file|File[A-Z]|zndFile|decompress|lzs"),
    ("Game.Event", r"script|event|opcode|cutscene|dialog|textbox|message"),
    ("Game.Items", r"item|weapon|blade|grip|gem|armor|shield|inventory|equip|material|accessor|container|loot|chest"),
    ("Game.Menu", r"menu|cursor|window|_ui|ui[A-Z]"),
    ("Game.Effects", r"effect|particle|_fx|Fx[A-Z]|plg|purge"),
    ("Game.Actors", r"actor|anim|skeleton|bone|model|shp|seq[A-Z]|wep\b|character|npc|enemy"),
    ("Game.Field", r"map|room|zone|znd|mpd|zud|door|trap|field|scene|camera|collision|floor|terrain"),
    ("Game.Battle", r"battle|attack|damage|skill|chain|risk|ability|spell|magic|hp|mp|status|target|combat|defen|breakArt|hit"),
    ("Graphics", r"draw|render|prim|sprite|texture|tex|clut|vram|tpage|fade|font|glyph|polygon|poly|ot\b|otag|screen|display|image|light|shade|color|colour"),
    ("Core.Math", r"matrix|rot|vector|vec|sqrt|atan|sin|cos|rand|random|lerp|interp|fixed"),
    ("Platform", r"vsync|frame|timer|clock|heap|alloc|free|memcpy|memset|init(?:Sys|System)|sysInit|exec|jump|overlay"),
]
NAME_RULES = [(s, re.compile(r, re.I)) for s, r in NAME_RULES]

UNIT_FALLBACK = {
    "TITLE": "Game.Title",
    "MENU": "Game.Menu",
    "EFFECT": "Game.Effects",
    "ENDING": "Game.Ending",
    "GIM": "Game.Effects",
}
API_TO_SYSTEM = {
    "libgpu": "Graphics", "libgte": "Core.Math", "inline_c": "Core.Math", "gtemac": "Core.Math",
    "libspu": "Audio", "libsnd": "Audio", "libcd": "Content", "libds": "Content", "libpress": "Video",
    "libpad": "Input", "libcard": "Save", "libapi": "Platform", "libetc": "Platform", "kernel": "Platform",
    "libc": "Core", "crt": "Core", "libsn": "Platform",
}


def classify(name: str, unit: str, file_rel: str, apis: collections.Counter) -> tuple[str, str]:
    if not name.startswith("func_"):
        for system, rx in NAME_RULES:
            if rx.search(name):
                if unit == "TITLE" and system in ("Graphics", "Game.Menu", "Platform"):
                    return "Game.Title", "name+unit"
                return system, "name"
    if apis:
        lib, _ = apis.most_common(1)[0]
        sys_ = API_TO_SYSTEM.get(lib, "Triage")
        if unit in UNIT_FALLBACK and sys_ in ("Graphics", "Core.Math", "Platform"):
            return UNIT_FALLBACK[unit], "unit(api:" + lib + ")"
        if unit == "BATTLE" and sys_ in ("Graphics", "Core.Math"):
            return "Game.Battle", "unit(api:" + lib + ")"
        return sys_, "api:" + lib
    if unit in UNIT_FALLBACK:
        return UNIT_FALLBACK[unit], "unit"
    if unit == "BATTLE":
        return "Game.Battle", "unit-low"
    fn = file_rel.lower()
    if "znd" in fn:
        return "Game.Field", "file"
    if "overlay" in fn:
        return "Platform", "file"
    return "Triage", "none"


# --------------------------------------------------------------------------- scan

def scan(sdk: dict[str, dict]):
    funcs: list[dict] = []
    files: list[dict] = []
    dsyms = collections.defaultdict(collections.Counter)
    sdk_use = collections.defaultdict(lambda: {"uses": 0, "files": set(), "units": collections.Counter()})

    for p in sorted(SRC.rglob("*")):
        if p.suffix not in (".c", ".h", ".s"):
            continue
        rel = p.relative_to(SRC)
        sdk_lib = is_sdk_path(rel)
        unit, overlay = unit_of(rel)
        raw = read(p)
        text = strip_comments(raw) if p.suffix != ".s" else raw
        frow = {
            "file": rel.as_posix(), "unit": unit, "overlay": overlay, "kind": p.suffix[1:],
            "sdk": sdk_lib or "", "lines": raw.count("\n") + 1, "funcs": 0, "generic": 0, "asm_funcs": 0,
            "d_refs": len(re.findall(r"\bD_[0-9A-F]{8}\b", text)), "unk": len(re.findall(r"\bunk\w*", text)),
        }
        for hk, rx in HAZARDS.items():
            frow[hk] = len(rx.findall(text)) if p.suffix != ".s" else 0
        if p.suffix == ".s":
            for name in GLABEL.findall(raw):
                funcs.append(dict(name=name, unit=unit, overlay=overlay, file=frow["file"], line=0, kind="asm-file",
                                  sdk=sdk_lib or "", lines=0, body=""))
                frow["asm_funcs"] += 1
            files.append(frow)
            continue
        for m in INCLUDE_ASM.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            funcs.append(dict(name=m.group(2), unit=unit, overlay=overlay, file=frow["file"], line=line,
                              kind="INCLUDE_ASM", sdk=sdk_lib or "", lines=0, body=""))
            frow["asm_funcs"] += 1
        if p.suffix in (".c", ".h"):
            for m in FUNC_HDR.finditer(text):
                name = m.group("name")
                if name in KEYWORDS:
                    continue
                brace = m.end() - 1
                end = find_body(text, brace)
                body = text[brace:end + 1]
                line = text.count("\n", 0, m.start()) + 1
                kind = "C" if p.suffix == ".c" else ("template-body" if unit == "EFFECT" and name.startswith("VS_") else "C-header")
                funcs.append(dict(name=name, unit=unit, overlay=overlay, file=frow["file"], line=line, kind=kind,
                                  sdk=sdk_lib or "", lines=body.count("\n") + 1, body=body))
                frow["funcs"] += 1
                frow["generic"] += name.startswith("func_")
        if not sdk_lib:
            for d in re.findall(r"\bD_[0-9A-F]{8}\b", text):
                dsyms[d][overlay] += 1
            for m in re.finditer(r"\b([A-Za-z_]\w*)\b(\s*\()?", text):
                ident = m.group(1)
                info = sdk.get(ident)
                if not info:
                    continue
                if info["kind"] != "constant" and not m.group(2):
                    continue
                u = sdk_use[ident]
                u["uses"] += 1
                u["files"].add(frow["file"])
                u["units"][overlay] += 1
            if unit == "EFFECT" and p.suffix == ".c":
                tmpl = re.findall(r'#include\s+"src/EFFECT/(\w+)\.h"', text)
                for m in re.finditer(r"^\s*#define\s+(VS_\w+_FUNCTION)\s+(\w+)", text, re.M):
                    line = text.count("\n", 0, m.start()) + 1
                    funcs.append(dict(name=m.group(2), unit=unit, overlay=overlay, file=frow["file"], line=line,
                                      kind="template:" + "+".join(tmpl) + ":" + m.group(1), sdk="", lines=0, body=""))
                    frow["funcs"] += 1
                    frow["generic"] += m.group(2).startswith("func_")
        files.append(frow)
    return funcs, files, dsyms, sdk_use


# --------------------------------------------------------------------------- outputs

def load_existing(path: Path, key) -> dict:
    if not path.exists():
        return {}
    with path.open(newline="", encoding="utf-8") as f:
        return {key(r): r for r in csv.DictReader(f)}


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    sdk = load_sdk()
    funcs, files, dsyms, sdk_use = scan(sdk)

    game_names = {f["name"] for f in funcs if not f["sdk"]}
    callers = collections.Counter()
    for f in funcs:
        f["callees"] = set()
        f["apis"] = collections.Counter()
        f["api_names"] = set()
        f["hazards"] = {k: len(rx.findall(f["body"])) for k, rx in HAZARDS.items()}
        if not f["body"]:
            continue
        for c in CALL.findall(f["body"]):
            if c == f["name"]:
                continue
            if c in game_names:
                f["callees"].add(c)
            elif c in sdk:
                lib = "libpress" if VIDEO_NAMES.match(c) else sdk[c]["lib"]
                f["apis"][lib] += 1
                f["api_names"].add(c)
        for c in f["callees"]:
            callers[c] += 1

    # FUNCTION_CATALOG.csv
    cat_path = OUT / "FUNCTION_CATALOG.csv"
    prev = load_existing(cat_path, lambda r: (r["overlay"], r["name"], r["file"]))
    cols = ["id", "overlay", "unit", "file", "line", "name", "kind", "generic", "lines", "proposed_system",
            "basis", "copy", "callers", "callees", "sdk_apis", *HAZARDS.keys(), "final_name", "final_location",
                        "purpose", *STATUS_COLS]
    rows = []
    ordered = sorted(funcs, key=lambda x: (x["sdk"] != "", x["unit"], x["overlay"], x["file"], x["line"], x["name"]))
    for i, f in enumerate(ordered):
        if f["sdk"]:
            system, basis, copy = "Replaced:" + SDK_TARGET.get(f["sdk"], ("Platform", ""))[0], "sdk", "no"
        else:
            system, basis = classify(f["name"], f["unit"], f["file"], f["apis"])
            copy = "yes"
        row = {
            "id": f"F{i:05d}", "overlay": f["overlay"], "unit": f["unit"], "file": f["file"], "line": f["line"],
            "name": f["name"], "kind": f["kind"], "generic": int(f["name"].startswith("func_")), "lines": f["lines"],
            "proposed_system": system, "basis": basis, "copy": copy, "callers": callers.get(f["name"], 0),
            "callees": len(f["callees"]), "sdk_apis": " ".join(sorted(f["api_names"])), **f["hazards"],
        }
        old = prev.get((row["overlay"], row["name"], row["file"])) or {}
        for c in [*STATUS_COLS, "final_name", "final_location", "purpose"]:
            row[c] = old.get(c, "")
        rows.append(row)
    with cat_path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)

    # SDK_API_LEDGER.md
    by_lib = collections.defaultdict(list)
    for name, u in sdk_use.items():
        lib = "libpress" if VIDEO_NAMES.match(name) else sdk[name]["lib"]
        by_lib[lib].append((name, u))
    lines = [
        "# PS1 SDK API Ledger (retirement list)",
        "",
        "Generated by `tools/ledger/build_ledgers.py`. Every PS1 SDK function or macro referenced by **game** code",
        "(SDK implementation folders excluded). **None of these is reimplemented as a PS1 API.** Each one is retired",
        "by rewriting its call sites to the modern system API in the *Target* line. During Phase 1 a call may go",
        "through a **temporary adapter** (see the plan, Â§7); the row only closes when its use count in `Native/` is 0.",
        "",
        "Status legend: `[ ]` not started, `[A]` temporary shim exists, `[M]` all call sites migrated, `[x]` shim deleted and 0 uses.",
        "",
        "Strategy: **R** rewrite call sites directly to the modern API during the copy (no shim ever exists);",
        "**T** temporary shim in `Native/<System>/Legacy/`, translating to the modern API, deleted at 0 uses;",
        "**D** delete the call (no meaning without PS1 hardware: sync, IRQs, kernel events, debug font...).",
        "",
        f"**{len(sdk_use)} SDK symbols are referenced by game code.**",
        "",
    ]
    strat_tot = collections.Counter()
    for lib in sorted(by_lib, key=lambda l: -sum(u["uses"] for _, u in by_lib[l])):
        sysname, target = SDK_TARGET.get(lib, ("Platform", "VS.Platform"))
        items = sorted(by_lib[lib], key=lambda x: (-x[1]["uses"], x[0]))
        uses = sum(u["uses"] for _, u in items)
        lines += [f"## {lib} â†’ {sysname} ({len(items)} symbols, {uses} uses)", "", f"Target: {target}", "",
                  "| Status | Strategy | Symbol | Kind | Uses | Files | Overlays (uses) |", "|---|---|---|---|---:|---:|---|"]
        for name, u in items:
            st = strategy_of(name, lib)
            strat_tot[st] += 1
            ov = ", ".join(f"{k} ({v})" for k, v in u["units"].most_common())
            lines.append(f"| [ ] | {st} | `{name}` | {sdk[name]['kind']} | {u['uses']} | {len(u['files'])} | {ov} |")
        lines.append("")
    lines.insert(14, f"Strategy totals: R={strat_tot['R']}, T={strat_tot['T']}, D={strat_tot['D']}.\n")
    (OUT / "SDK_API_LEDGER.md").write_text("\n".join(lines), encoding="utf-8")

    # SOURCE_FILE_LEDGER.md
    sys_by_file = collections.defaultdict(collections.Counter)
    for r in rows:
        if r["copy"] == "yes":
            sys_by_file[r["file"]][r["proposed_system"]] += 1
    lines = [
        "# Source File Ledger (copy â†’ build â†’ name â†’ modernize â†’ C++ â†’ verify)",
        "",
        "Generated by `tools/ledger/build_ledgers.py`. One row per decomp file. SDK implementation files are listed",
        "as **not copied**: their behavior is replaced by modern systems (see `SDK_API_LEDGER.md`).",
        "",
        "Counts: *Fn* functions, *Gen* still `func_`, *ASM* functions only in asm, *HW* hardware-register literals,",
        "*SP* scratchpad, *KSEG* 0x8xxxxxxx literals, *OT* 24-bit tag idioms, *Cast* intâ†”pointer casts, *GTE* inline asm/GTE macros.",
        "",
        "Status: **C** copied into `Native/`, **B** builds, **N** all functions named, **M** modernized (no adapters,",
        "no hazards), **P** promoted to `.cpp`, **V** verified against the original behavior.",
        "",
    ]
    for unit in sorted({f["unit"] for f in files}):
        uf = [f for f in files if f["unit"] == unit]
        lines += [f"## {unit}", "",
                  "| C | B | N | M | P | V | File | Lines | Fn | Gen | ASM | HW | SP | KSEG | OT | Cast | GTE | D_ | unk | Systems |",
                  "|---|---|---|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|"]
        for f in sorted(uf, key=lambda x: x["file"]):
            if f["sdk"]:
                systems = "**not copied** â†’ " + SDK_TARGET.get(f["sdk"], ("Platform", ""))[0]
            else:
                systems = ", ".join(f"{k} {v}" for k, v in sys_by_file[f["file"]].most_common(4))
            lines.append(
                f"| [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | `{f['file']}` | {f['lines']} | {f['funcs']} | {f['generic']} | "
                f"{f['asm_funcs']} | {f['hw_reg']} | {f['scratchpad']} | {f['kseg_literal']} | {f['ot24']} | "
                f"{f['ptr_cast']} | {f['inline_asm']} | {f['d_refs']} | {f['unk']} | {systems} |")
        lines.append("")
    (OUT / "SOURCE_FILE_LEDGER.md").write_text("\n".join(lines), encoding="utf-8")

    # DATA_LEDGER.csv
    named = {}
    for sf in CONFIG.rglob("symbol_addrs.txt"):
        ov = sf.parent.name
        for ln in read(sf).splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;\s*(?://\s*(.*))?", ln)
            if m and m.group(1) not in game_names and m.group(1) not in sdk and not m.group(1).startswith("jtbl_"):
                named[(ov, m.group(1))] = (m.group(2), (m.group(3) or "").strip())
    data_path = OUT / "DATA_LEDGER.csv"
    prev = load_existing(data_path, lambda r: (r["overlay"], r["symbol"]))
    dcols = ["overlay", "symbol", "address", "attrs", "uses", "origin", "proposed_contract", "final_name", *STATUS_COLS]
    drows = []
    for (ov, sym), (addr, attrs) in sorted(named.items()):
        drows.append(dict(overlay=ov, symbol=sym, address=addr, attrs=attrs, uses="", origin="symbol_addrs"))
    for sym, ctr in sorted(dsyms.items()):
        for ov, n in sorted(ctr.items()):
            drows.append(dict(overlay=ov, symbol=sym, address="0x" + sym[2:], attrs="", uses=n, origin="D_ reference"))
    for r in drows:
        old = prev.get((r["overlay"], r["symbol"])) or {}
        for c in [*STATUS_COLS, "proposed_contract", "final_name"]:
            r[c] = old.get(c, "")
    with data_path.open("w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=dcols)
        w.writeheader()
        w.writerows(drows)

    # SUMMARY.md
    game_rows = [r for r in rows if r["copy"] == "yes"]
    by_sys = collections.Counter(r["proposed_system"] for r in game_rows)
    by_basis = collections.Counter(re.split(r"[(:]", r["basis"])[0] for r in game_rows)
    by_ov = collections.defaultdict(lambda: [0, 0, 0])
    for r in game_rows:
        by_ov[r["overlay"]][0] += 1
        by_ov[r["overlay"]][1] += int(r["generic"])
        by_ov[r["overlay"]][2] += r["kind"] in ("INCLUDE_ASM", "asm-file")
    kinds = collections.Counter(r["kind"].split(":")[0] for r in game_rows)
    s = ["# Ledger Summary", "", "Generated by `tools/ledger/build_ledgers.py` from `resources/rood-reverseWin` (read-only).", "",
         f"- Game function rows: **{len(game_rows)}** (" + ", ".join(f"{k}: {v}" for k, v in kinds.most_common()) + ")",
         f"- Still generic (`func_`): **{sum(int(r['generic']) for r in game_rows)}**",
         f"- SDK functions not copied (replaced): **{len(rows) - len(game_rows)}**",
         f"- SDK symbols referenced by game code: **{len(sdk_use)}** (see SDK_API_LEDGER.md)",
         f"- Data symbol rows: **{len(drows)}** ({len(named)} named, {len(dsyms)} unique `D_`)",
         f"- Files: **{len(files)}** ({sum(1 for f in files if not f['sdk'])} game, {sum(1 for f in files if f['sdk'])} SDK)",
         "", "## Proposed system (auto-classified; `Triage` = needs a decision)", "",
         "| System | Functions |", "|---|---:|"]
    s += [f"| {k} | {v} |" for k, v in by_sys.most_common()]
    s += ["", "## Classification basis", "", "| Basis | Functions |", "|---|---:|"]
    s += [f"| {k} | {v} |" for k, v in by_basis.most_common()]
    s += ["", "## Per overlay", "", "| Overlay | Functions | Generic | Asm-only |", "|---|---:|---:|---:|"]
    s += [f"| {k} | {v[0]} | {v[1]} | {v[2]} |" for k, v in sorted(by_ov.items())]
    hz = collections.Counter()
    for r in game_rows:
        for k in HAZARDS:
            hz[k] += int(r[k])
    s += ["", "## Hazards inside game functions", "", "| Hazard | Hits |", "|---|---:|"]
    s += [f"| {k} | {v} |" for k, v in hz.items()]
    (OUT / "SUMMARY.md").write_text("\n".join(s) + "\n", encoding="utf-8")
    print("\n".join(s))
    return 0


if __name__ == "__main__":
    sys.exit(main())
