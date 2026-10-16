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
        named: list[tuple[str, int, int, str]] = []               # (name, file_off, vram_delta, kind) asset segments
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
                    nm = sub[2] if len(sub) > 2 and isinstance(sub[2], str) else None
                elif isinstance(sub, dict):
                    off, kind = int(sub["start"]), sub.get("type", "data")
                    nm = sub.get("name")
                else:
                    continue
                flat.append((off, vram - start, str(kind)))
                if nm and str(kind) not in ("c", "asm", "hasm", "data", "rodata", "bss", ".data", ".rodata", ".bss"):
                    named.append((Path(nm).name, off, vram - start, str(kind)))
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
        for sa in sorted(cfg.parent.glob("symbol_addrs*.txt")):
            for line in sa.read_text(encoding="utf-8", errors="ignore").splitlines():
                m = SYM_LINE.match(line)
                if m:
                    self.symbols.setdefault(m.group(1), (int(m.group(2), 16), m.group(3) or ""))
        # Named asset subsegments (images, CLUTs, ...) define their symbol at the segment start.
        for nm, off, delta, kind in named:
            self.symbols.setdefault(nm, (off + delta, f"asset:{kind}"))

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
        elif kind not in ("data", "rodata", "bss", "end"):
            kind = "asset"  # splat asset segment (image, CLUT, RLE bitmap, ...): converted by VS.Extract (Tier 3)
        return kind, (off + addr - v0) if kind in ("data", "rodata", "unit-data", "asset") else None


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


RESIDENT = ("SLUS_010.40", "BATTLE/BATTLE.PRG")  # loaded while other overlays run, so their storage is shared


def main() -> int:
    idx = json.loads(INDEX.read_text(encoding="utf-8"))
    overlays = load_overlays()
    types_by_key = collections.defaultdict(list)
    for t in idx["types"].values():
        types_by_key[t["key"]].append(t)

    # address -> sorted list per overlay of all known symbol addresses (for gap sizing)
    addr_lists: dict[str, list[int]] = {k: sorted({a for a, _ in ov.symbols.values()}) for k, ov in overlays.items()}

    def address_of(name: str, prefer: list[str]) -> tuple[int | None, str]:
        m = D_NAME.match(name)
        if m:
            return int(m.group(1), 16), ""
        for k in prefer + [k for k in overlays if k not in prefer]:
            if name in overlays[k].symbols:
                return overlays[k].symbols[name]
        return None, ""

    def owner_of(addr: int | None, decl_ov: str | None) -> str | None:
        """Which executable's storage an extern refers to, seen from a declaring file in `decl_ov`."""
        if addr is None:
            return None
        order = ([decl_ov] if decl_ov in overlays else []) + [k for k in RESIDENT if k != decl_ov]
        for k in order:
            if overlays[k].contains(addr):
                return k
        hits = [k for k, ov in overlays.items() if ov.contains(addr)]
        return hits[0] if len(hits) == 1 else None

    groups: dict[str, dict] = {}

    def put(key: str, name: str, which: str, rec: dict, instances: set[str] | None = None) -> None:
        g = groups.setdefault(key, {"name": name, "defs": [], "decls": [], "instances": set()})
        g[which].append(rec)
        if instances:
            g["instances"] |= instances

    def tu_overlays(d: dict) -> set[str]:
        return {o for o in (overlay_key_of_file(t) for t in d.get("tus", [])) if o}

    def home(d: dict) -> tuple[str | None, set[str]]:
        """(overlay, instances): the overlay a declaration belongs to, or None plus the including overlays when it
        sits in a header shared by several overlays (effect/menu templates): one name, storage per instance."""
        ov = overlay_key_of_file(d["file"])
        if ov:
            return ov, set()
        tov = tu_overlays(d)
        if len(tov) == 1:
            return next(iter(tov)), set()
        return None, tov

    for name, g in idx["globals"].items():
        nonstatic_ovs: dict[str, bool] = {}
        for d in g["defs"]:
            ov, inst = home(d)
            if d["static"] or ov is None:
                put(f"{name}@{d['file']}" + (f":{d['scope']}" if d.get("scope") else ""), name, "defs", d, inst)
            else:
                nonstatic_ovs[ov] = True
                put(f"{name}@{ov}", name, "defs", d)
        for d in g["decls"]:
            dov, inst = home(d)
            if dov is None:
                put(f"{name}@{d['file']}", name, "decls", d, inst)
                continue
            if dov in nonstatic_ovs:
                owner = dov
            elif len(nonstatic_ovs) == 1 and next(iter(nonstatic_ovs)) in RESIDENT:
                owner = next(iter(nonstatic_ovs))
            else:
                addr, _ = address_of(name, [dov] + list(RESIDENT))
                owner = owner_of(addr, dov) or dov
            put(f"{name}@{owner}", name, "decls", d)
    rows = []
    stats = collections.Counter()
    for key, g in sorted(groups.items()):
        name = g["name"]
        recs = g["defs"] + g["decls"]
        first = (g["defs"] or g["decls"])[0]
        rec = {"key": key, "name": name, "files": sorted({d["file"] for d in recs}), "type": first["type"]}
        scopes = sorted({d["scope"] for d in recs if d.get("scope")})
        if scopes:
            rec["scopes"] = scopes
        tkey = next((d.get("type_key") for d in recs if d.get("type_key")), None)
        if tkey:
            rec["type_key"] = tkey
        tsize = max([d.get("size", -1) for d in recs] + [-1])
        rec["has_pointers"] = type_has_pointers(rec["type"], tkey, types_by_key)
        owner = key.split("@", 1)[1]
        if g["instances"]:
            rec["instances"] = sorted(g["instances"])
            owner = rec["instances"][0]
        if g["defs"]:
            d0 = g["defs"][0]
            local = bool(d0.get("scope")) and d0["static"]
            rec.update(segment="c-defined", overlay=home(d0)[0], size=tsize,
                       static=d0["static"], init=any(d.get("init") for d in g["defs"]),
                       linkage="per-instance" if g["instances"] else
                       ("local-static" if local else ("file-static" if d0["static"] else "extern")))
            stats["c-defined"] += 1
            rows.append(rec)
            continue
        rec["linkage"] = "per-instance" if g["instances"] else "extern"
        ov_key = owner if owner in overlays else None
        addr, attrs = address_of(name, ([ov_key] if ov_key else []) + list(RESIDENT))
        if addr is None or ov_key is None:
            rec.update(segment="unknown", overlay=ov_key, size=tsize)
            stats["unknown"] += 1
            rows.append(rec)
            continue
        seg, foff = overlays[ov_key].segment_of(addr)
        size = tsize
        sm = re.search(r"size:(0x[0-9A-Fa-f]+|\d+)", attrs)
        if sm:
            size = int(sm.group(1), 0)
        if size is None or size <= 0:
            lst = addr_lists.get(ov_key, [])
            i = bisect.bisect_right(lst, addr)
            size = (lst[i] - addr) if i < len(lst) else -1
        rec.update(segment=seg, overlay=ov_key, address=f"0x{addr:08X}", file_offset=foff, size=size)
        stats[seg] += 1
        rows.append(rec)

    OUT.write_text(json.dumps(rows, indent=1), encoding="utf-8")
    ptr = collections.Counter((r["segment"], r["has_pointers"]) for r in rows)
    split = collections.Counter(r["name"] for r in rows)
    print(f"{len(rows)} storage identities ({len(idx['globals'])} spellings) -> {OUT}")
    for k, v in stats.most_common():
        print(f"  {k:12} {v}")
    print("  linkage:", dict(collections.Counter(r["linkage"] for r in rows)))
    print("  (segment, has_pointers):", dict(ptr))
    print(f"  spellings with >1 identity: {sum(1 for v in split.values() if v > 1)}")
    unsized = [r["key"] for r in rows if r["segment"] in ("data", "rodata", "bss") and (r["size"] or -1) <= 0]
    print(f"  unsized data/bss: {len(unsized)} e.g. {unsized[:8]}")
    unknown = [r["key"] for r in rows if r["segment"] == "unknown"]
    print(f"  unknown: {unknown[:20]}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
