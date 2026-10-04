#!/usr/bin/env python3
"""Harvest read-only data tables from the retail overlays into the native build.

Some data symbols the render path reads (menu string tables, glyph-width tables,
colour tables, …) are not yet decompiled into C, so the native stub layer backs
them with zero blobs. Zeroed string tables have no 0xFF terminator and send the
glyph loop into a runaway. This tool copies the real bytes out of the retail
overlay binaries at their known symbol addresses and emits an initialised C
definition, so the native build links the genuine data.

Only symbols named on the command line are harvested (a curated list -- we do
NOT blanket-copy, because most stubbed "data" symbols are genuine .bss runtime
state that must stay zero). A symbol whose address lands beyond its overlay's
file image is .bss and is skipped (left to the zero stub).

Usage: harvest_data.py <out.c> <out.syms> <sym1> [sym2 ...]

Run from the repo root. Overlay VRAM bases come from each config/**/splat.yaml.
The emitted .syms file lists what was actually harvested, so gen_stubs.py can
exclude those names from the zero-blob stub layer.
"""
import os, sys, glob, re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get("VS_DECOMP_SOURCE", REPO / "resources" / "rood-reverseWin"))
DATA = Path(os.environ.get("VS_DATA_DIR", REPO / "data"))

# Cap a single symbol's harvested span, in case the next symbol is far away
# (e.g. the last symbol before a big gap). Menu tables are all well under this.
SIZE_CAP = 0x2000


def overlays():
    """(vram, binary_path, {name: addr}) for every overlay with a splat.yaml.

    `vram` is returned already adjusted so that `addr - vram` is a file offset.
    The overlays load at file offset 0, but SLUS_010.40 is a PS-EXE: its code
    segment starts 0x800 bytes in, past the header, and its splat.yaml says so
    with `start: 0x800`. Ignoring that reads every main-executable symbol 0x800
    bytes early -- which is not a crash, just the wrong table, so it shows up
    much later as (for instance) a semitone table whose first entry is 0 and
    therefore every note playing at zero pitch."""
    out = []
    for y in sorted(glob.glob(str(SOURCE / "config" / "**" / "splat.yaml"), recursive=True)):
        txt = open(y).read()
        # The code segment carries both the file offset and the vram it maps to.
        seg = re.search(r"start:\s*(0x[0-9A-Fa-f]+)\s*\n\s*type:\s*code\s*\n"
                        r"\s*vram:\s*(0x[0-9A-Fa-f]+)", txt)
        vram = re.search(r"vram:\s*(0x[0-9A-Fa-f]+)", txt)
        target = re.search(r"target_path:\s*(\S+)", txt)
        if not (vram and target):
            continue
        if seg:
            vram = int(seg.group(2), 16) - int(seg.group(1), 16)
        else:
            vram = int(vram.group(1), 16)
        binpath = str(DATA / Path(target.group(1)).relative_to("data"))
        # Primary symbol map for this overlay: config/<same dirs>/symbol_addrs.txt
        symfile = os.path.join(os.path.dirname(y), "symbol_addrs.txt")
        syms = {}
        if os.path.exists(symfile):
            for line in open(symfile):
                m = re.match(r"\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)", line)
                if m:
                    syms[m.group(1)] = int(m.group(2), 16)
        out.append((vram, binpath, syms))
    return out


def find_home(name, ovls):
    """Overlay (vram, bin, syms) whose own symbol map defines `name`."""
    for vram, binpath, syms in ovls:
        if name in syms:
            return vram, binpath, syms
    return None


def harvest_one(name, ovls):
    home = find_home(name, ovls)
    if not home:
        return None, "no address in any overlay symbol map"
    vram, binpath, syms = home
    addr = syms[name]
    if not os.path.exists(binpath):
        return None, "overlay binary missing: " + binpath
    data = open(binpath, "rb").read()
    off = addr - vram
    if off < 0 or off >= len(data):
        return None, ".bss (beyond file image) -- left zeroed"
    # size = gap to the next symbol in the SAME overlay, capped.
    higher = sorted(a for a in syms.values() if a > addr)
    size = (higher[0] - addr) if higher else SIZE_CAP
    size = max(1, min(size, SIZE_CAP, len(data) - off))
    return (data[off:off + size], addr, binpath), None


def register_auto_names(names, ovls):
    """Auto-named `D_<8 hex>` symbols encode their own address but are absent
    from the friendly symbol_addrs.txt maps. Inject each into the overlay whose
    image contains that address so find_home resolves it and the gap-based size
    computation sees it as a boundary. Only injects when the address actually
    lands inside an overlay's file image."""
    for name in names:
        m = re.match(r"^D_([0-9A-Fa-f]{8})$", name)
        if not m:
            continue
        if any(name in syms for _, _, syms in ovls):
            continue
        addr = int(m.group(1), 16)
        for vram, binpath, syms in ovls:
            if not os.path.exists(binpath):
                continue
            if vram <= addr < vram + len(open(binpath, "rb").read()):
                syms[name] = addr
                break


def main(out_c, out_syms, names):
    ovls = overlays()
    register_auto_names(names, ovls)
    blocks, harvested = [], []
    report = []
    for name in names:
        res, err = harvest_one(name, ovls)
        if res is None:
            report.append("  SKIP  %-32s %s" % (name, err))
            continue
        blob, addr, binpath = res
        harvested.append(name)
        body = ",".join(str(b) for b in blob)
        blocks.append(
            "/* %s @ 0x%08X, %d bytes from %s */\n"
            "unsigned char %s[%d] __attribute__((aligned(16))) = {%s};"
            % (name, addr, len(blob), os.path.basename(binpath), name, len(blob), body))
        report.append("  HARVEST %-30s 0x%08X  %d bytes" % (name, addr, len(blob)))

    header = ("/* AUTO-GENERATED by native/tools/harvest_data.py -- do not hand-edit.\n"
              " * Real read-only data copied from the retail overlays. */\n")
    open(out_c, "w").write(header + "\n" + "\n\n".join(blocks) + "\n")
    open(out_syms, "w").write("\n".join(harvested) + ("\n" if harvested else ""))
    print("  HARVEST %s (%d symbols)" % (out_c, len(harvested)))
    print("\n".join(report))


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3:])
