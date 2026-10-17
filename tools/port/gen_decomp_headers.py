"""Generate the decomp's build-time headers so the decomp can be *parsed* (analysis only).

The decomp includes headers that its own build generates from the disc:
  build/src/include/lbas.h              VS_<FILE>_<EXT>_LBA / _SIZE for every disc file
  build/assets/**/<stem>.h              string-table offset/index enums (from assets/**/*.yaml)
  build/assets/**/<stem>.vsString       string-table data spliced into array initializers

They are written to build/port/gen/ (git-ignored). The copy tool turns the disc layout and the string ids into
Content/DiscLayout.h and Content/StringTables.h: positions only (sectors, sizes, string offsets read from the
user's extracted data), no disc contents.

Usage: python tools/port/gen_decomp_headers.py [--disc disks/SLUS-01040.cue] [--data Native/data]
"""
from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[2]
DECOMP = REPO / "resources" / "rood-reverseWin"
GEN = REPO / "build" / "port" / "gen"


def disc_listing(disc: Path) -> list[tuple[int, int, str]]:
    exe = REPO / "build" / "Debug" / "VS.Extract.exe"
    if not exe.exists():
        exe = REPO / "build" / "Release" / "VS.Extract.exe"
    if not exe.exists():
        sys.exit("VS.Extract.exe not built; run build-native.cmd first")
    out = subprocess.run([str(exe), "list", str(disc)], capture_output=True, text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        parts = line.split(None, 3)
        if len(parts) != 4 or not parts[0].isdigit() or not parts[1].isdigit() or parts[2].startswith("d"):
            continue
        rows.append((int(parts[0]), int(parts[1]), parts[3]))
    return rows


def write_lbas(disc: Path) -> int:
    seen: dict[str, tuple[int, int]] = {}
    for lba, size, path in disc_listing(disc):
        name = path.rsplit("/", 1)[-1].replace(".", "_")
        seen.setdefault(name, (lba, (size + 2047) // 2048 * 2048))
    out = GEN / "build" / "src" / "include" / "lbas.h"
    out.parent.mkdir(parents=True, exist_ok=True)
    lines = ["#pragma once"]
    lines += [f"#define VS_{n}_LBA {lba}" for n, (lba, _) in seen.items()]
    lines += [f"#define VS_{n}_SIZE {size}" for n, (_, size) in seen.items()]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return len(seen)


def string_table_bytes(rel: Path, data: Path) -> bytes | None:
    """The bytes a string table yaml describes, from the extracted disc data: a whole file (assets/MENU/MCMAN.BIN.yaml
    is MENU/MCMAN.BIN) or a vsStringTable segment of a module image (assets/MENU/MENU0.PRG/magic.yaml is the
    `magic` segment of MENU/MENU0.PRG in its splat config)."""
    whole = data / "disc" / rel
    if whole.is_file():
        return whole.read_bytes()
    module = data / "disc" / rel.parent
    cfg = DECOMP / "config" / rel.parent / "splat.yaml"
    if not module.is_file() or not cfg.is_file():
        return None
    for line in cfg.read_text(encoding="utf-8").splitlines():
        m = (re.search(r"\[\s*(0x[0-9A-Fa-f]+)\s*,\s*vsStringTable\s*,\s*(\w+)\s*\]", line) or
             re.search(r"start:\s*(0x[0-9A-Fa-f]+)\s*,\s*type:\s*vsStringTable\s*,\s*name:\s*(\w+)", line))
        if m and m[2] == rel.name:
            return module.read_bytes()[int(m[1], 16):]
    return None


def write_strings(data: Path) -> tuple[int, int]:
    """Offsets and indices as the decomp's build computes them (tools/etc/vsString_yaml.py): a key's index is the
    position of its first string (list values hold several), and its offset, in halfwords from the table start, is
    the table's entry at that index. The yaml holds only the keys, so the offsets come from the table on the disc."""
    count, from_disc = 0, 0
    for y in sorted((DECOMP / "assets").rglob("*.yaml")):
        rel = y.relative_to(DECOMP / "assets").with_suffix("")
        stem = rel.name.replace(".", "_")
        data_yaml = yaml.safe_load(y.read_text(encoding="utf-8")) or {}
        if not isinstance(data_yaml, dict):
            continue
        indices, n = {}, 0
        for k, v in data_yaml.items():
            indices[k] = n
            n += len(v) if isinstance(v, list) else 1
        table = string_table_bytes(Path(*rel.parts), data)
        offsets = None
        if table is not None and len(table) >= 2 * n and struct.unpack_from("<H", table, 0)[0] == n:
            offsets = {k: struct.unpack_from("<H", table, 2 * i)[0] for k, i in indices.items()}
            from_disc += 1
        elif table is not None:
            print(f"  {rel}: not an offset table of {n} strings; offsets left as indices")
        dest = GEN / "build" / "assets" / rel.parent / rel.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        body = [f"#pragma once\n\nenum {stem}_offsets_e {{"]
        body += [f"    VS_{stem}_OFFSET_{k} = {(offsets or indices)[k]}," for k in indices]
        body += ["};\n", f"enum {stem}_indices_e {{"]
        body += [f"    VS_{stem}_INDEX_{k} = {i}," for k, i in indices.items()]
        body += ["};"]
        Path(str(dest) + ".h").write_text("\n".join(body) + "\n", encoding="utf-8")
        Path(str(dest) + ".vsString").write_text("0\n", encoding="utf-8")
        count += 1
    return count, from_disc


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--disc", default=str(REPO / "disks" / "SLUS-01040.cue"))
    ap.add_argument("--data", default=str(REPO / "Native" / "data"), help="VS.Extract output (string table offsets)")
    a = ap.parse_args()
    n = write_lbas(Path(a.disc))
    s, d = write_strings(Path(a.data))
    print(f"lbas.h: {n} files; string headers: {s} ({d} with offsets from the disc) -> {GEN}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
