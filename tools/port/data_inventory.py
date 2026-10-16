"""Inventory every decomp global: where its storage and initial bytes come from.

The decomp defines only ~190 globals in C. The rest are `extern` in C and get their storage (and initial bytes)
from the original executables through splat `data`/`rodata`/`bss` segments. The native port must define all of
them, so for each global this records:

  overlay      which executable the symbol lives in (SLUS_010.40, BATTLE.PRG, MENU4.PRG, ...)
  address      its original load address (analysis only; never emitted into Native code)
  segment      c-defined | data | rodata | bss | unknown
  file_offset  byte offset of its initial bytes inside the overlay file on disc (data/rodata)
  size         bytes (type size under the original 32-bit layout, else gap to the next symbol)
  has_pointers whether the type contains pointers (initial bytes then need relocation, not a raw copy)

Inputs: build/port/index.json (decomp_index.py) + config/**/splat.yaml + config/**/symbol_addrs.txt.
Output: build/port/data.json

Usage: python tools/port/data_inventory.py
"""
from __future__ import annotations

import bisect
import collections
import json
import re
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[2]
DECOMP = REPO / "resources" / "rood-reverseWin"
CONFIG = DECOMP / "config"
INDEX = REPO / "build" / "port" / "index.json"
OUT = REPO / "build" / "port" / "data.json"

SYM_LINE = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;\s*(?://\s*(.*))?$")
D_NAME = re.compile(r"^D_([0-9A-F]{8})$")
DATA_KINDS = {"data": "data", ".data": "data", "databin": "data", "rodata": "rodata", ".rodata": "rodata",
              "bss": "bss", ".bss": "bss", ".sbss": "bss", "sbss": "bss",
              "vsStringTable": "rodata", "vsStringFixedLengthTable": "rodata"}


class Overlay:
    def __init__(self, cfg: Path) -> None:
        self.cfg = cfg
        d = yaml.safe_load(cfg.read_text(encoding="utf-8"))
        self.name = Path(d["options"]["basename"]).name          # SLUS_010.40, BATTLE.PRG, MENU4.PRG
        self.key = cfg.parent.relative_to(CONFIG).as_posix()      # SLUS_010.40, BATTLE/BATTLE.PRG
        self.ranges: list[tuple[int, int, str, int]] = []         # (vram_start, vram_end, kind, file_off)
        self.lo, self.hi = 1 << 32, 0
        segs = d.get("segments", [])
        flat: list[tuple[int, int | None, str]] = []              # (file_off, vram_base_or_None, kind)
        seg_bounds = []
        for s in segs:
            if isinstance(s, list):
                if len(s) == 1:
                    seg_bounds.append(int(s[0]))
                continue
            if not isinstance(s, dict) or s.get("type") != "code" or "vram" not in s:
                continue
            start, vram = int(s["start"]), int(s["vram"])
            seg_bounds.append(start)
            for sub in s.get("subsegments", []):
                if isinstance(sub, list):
                    off, kind = int(sub[0]), (sub[1] if len(sub) > 1 else "end")
                elif isinstance(sub, dict):
                    off, kind = int(sub["start"]), sub.get("type", "data")
                else:
                    continue
                flat.append((off, vram - start, str(kind)))
        flat.sort()
        ends = sorted(set(seg_bounds + [f[0] for f in flat]))
        for i, (off, delta, kind) in enumerate(flat):
            nxt = next((e for e in ends if e > off), None)
            if nxt is None:
                continue
            v0, v1 = off + delta, nxt + delta
            self.lo, self.hi = min(self.lo, v0), max(self.hi, v1)
            self.ranges.append((v0, v1, DATA_KINDS.get(kind, "code" if kind in ("c", "asm", "hasm") else kind), off))
        self.ranges.sort()
        self.starts = [r[0] for r in self.ranges]
        self.symbols: dict[str, tuple[int, str]] = {}
        sa = cfg.parent / "symbol_addrs.txt"
        if sa.exists():
            for line in sa.read_text(encoding="utf-8", errors="ignore").splitlines():
                m = SYM_LINE.match(line)
                if m:
                    self.symbols[m.group(1)] = (int(m.group(2), 16), m.group(3) or "")

    def contains(self, addr: int) -> bool:
        return self.lo <= addr < self.hi

    def segment_of(self, addr: int) -> tuple[str, int | None]:
        i = bisect.bisect_right(self.starts, addr) - 1
        if i < 0:
            return "unknown", None
        v0, v1, kind, off = self.ranges[i]
        if not (v0 <= addr < v1):
            return "unknown", None
        if kind == "code":
            # Data the decomp's linker script places inside a C/asm subsegment (rodata/data of that unit):
            # still backed by bytes in the overlay file.
            kind = "unit-data"
        return kind, (off + addr - v0) if kind in ("data", "rodata", "unit-data") else None


def load_overlays() -> dict[str, Overlay]:
    out = {}
    for cfg in sorted(CONFIG.rglob("splat.yaml")):
        if cfg.parent == CONFIG:
            continue
        ov = Overlay(cfg)
        out[ov.key] = ov
    return out


def overlay_key_of_file(f: str) -> str | None:
    if not f.startswith("src/"):
        return None
    parts = f[4:].split("/")
    if parts[0] == "SLUS_010.40":
        return "SLUS_010.40"
    if len(parts) >= 3:
        return f"{parts[0]}/{parts[1]}"
    return None


def type_has_pointers(type_spelling: str, type_key: str | None, types_by_key: dict, seen=None) -> bool:
    if "*" in type_spelling or "(" in type_spelling:
        return True
    if not type_key:
        return False
    seen = seen or set()
    if type_key in seen:
        return False
    seen.add(type_key)
    for t in types_by_key.get(type_key, []):
        for f in t["fields"]:
            ft = f.get("type", "")
            if "*" in ft or "(" in ft:
                return True
        for dep, by_value in t["deps"].items():
            if by_value and type_has_pointers("", dep, types_by_key, seen):
                return True
    return False


def main() -> int:
    idx = json.loads(INDEX.read_text(encoding="utf-8"))
    overlays = load_overlays()
    slus, battle = overlays.get("SLUS_010.40"), overlays.get("BATTLE/BATTLE.PRG")
    types_by_key = collections.defaultdict(list)
    for t in idx["types"].values():
        types_by_key[t["key"]].append(t)

    # address -> sorted list per overlay of all known symbol addresses (for gap sizing)
    addr_lists: dict[str, list[int]] = {}
    for k, ov in overlays.items():
        addrs = {a for a, _ in ov.symbols.values()}
        addr_lists[k] = sorted(addrs)

    rows = []
    stats = collections.Counter()
    for name, g in sorted(idx["globals"].items()):
        decl_files = [d["file"] for d in g["defs"] + g["decls"]]
        rec = {"name": name, "files": sorted(set(decl_files)), "type": (g["defs"] or g["decls"])[0]["type"]}
        tkey = next((d.get("type_key") for d in g["defs"] + g["decls"] if d.get("type_key")), None)
        tsize = max([d.get("size", -1) for d in g["defs"] + g["decls"]] + [-1])
        rec["has_pointers"] = type_has_pointers(rec["type"], tkey, types_by_key)
        if g["defs"]:
            rec.update(segment="c-defined", overlay=overlay_key_of_file(g["defs"][0]["file"]), size=tsize,
                       static=g["defs"][0]["static"], init=any(d.get("init") for d in g["defs"]))
            stats["c-defined"] += 1
            rows.append(rec)
            continue
        # find the address
        addr, ov_key, attrs = None, None, ""
        candidates = []
        for f in decl_files:
            k = overlay_key_of_file(f)
            if k and k in overlays and k not in candidates:
                candidates.append(k)
        for k in ("SLUS_010.40", "BATTLE/BATTLE.PRG"):
            if k not in candidates:
                candidates.append(k)
        m = D_NAME.match(name)
        if m:
            addr = int(m.group(1), 16)
        else:
            for k in candidates + [k for k in overlays if k not in candidates]:
                if name in overlays[k].symbols:
                    addr, attrs = overlays[k].symbols[name]
                    break
        if addr is None:
            rec.update(segment="unknown", overlay=None, size=tsize)
            stats["no-address"] += 1
            rows.append(rec)
            continue
        for k in candidates:
            if overlays[k].contains(addr):
                ov_key = k
                break
        if ov_key is None:
            ov_key = next((k for k, ov in overlays.items() if ov.contains(addr)), None)
        seg, foff = overlays[ov_key].segment_of(addr) if ov_key else ("unknown", None)
        size = tsize
        sm = re.search(r"size:(0x[0-9A-Fa-f]+|\d+)", attrs)
        if sm:
            size = int(sm.group(1), 0)
        if size is None or size <= 0:
            lst = addr_lists.get(ov_key or "", [])
            i = bisect.bisect_right(lst, addr)
            size = (lst[i] - addr) if i < len(lst) else -1
        rec.update(segment=seg, overlay=ov_key, address=f"0x{addr:08X}", file_offset=foff, size=size)
        stats[seg] += 1
        rows.append(rec)

    OUT.write_text(json.dumps(rows, indent=1), encoding="utf-8")
    ptr = collections.Counter((r["segment"], r["has_pointers"]) for r in rows)
    print(f"{len(rows)} globals -> {OUT}")
    for k, v in stats.most_common():
        print(f"  {k:12} {v}")
    print("  (segment, has_pointers):", dict(ptr))
    unsized = [r["name"] for r in rows if r["segment"] in ("data", "rodata", "bss") and (r["size"] or -1) <= 0]
    print(f"  unsized data/bss: {len(unsized)} e.g. {unsized[:8]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
