"""Generate the decomp's build-time headers so the decomp can be *parsed* (analysis only).

The decomp includes headers that its own build generates from the disc:
  build/src/include/lbas.h              VS_<FILE>_<EXT>_LBA / _SIZE for every disc file
  build/assets/**/<stem>.h              string-table offset/index enums (from assets/**/*.yaml)
  build/assets/**/<stem>.vsString       string-table data spliced into array initializers

They are written to build/port/gen/ (git-ignored). Nothing here is used by Native code:
Native code gets FileIds and string catalogs instead.

Usage: python tools/port/gen_decomp_headers.py [--disc disks/SLUS-01040.cue]
"""
from __future__ import annotations

import argparse
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


def write_strings() -> int:
    count = 0
    for y in sorted((DECOMP / "assets").rglob("*.yaml")):
        rel = y.relative_to(DECOMP / "assets").with_suffix("")
        stem = rel.name.replace(".", "_")
        data = yaml.safe_load(y.read_text(encoding="utf-8")) or {}
        if not isinstance(data, dict):
            continue
        keys = list(data.keys())
        dest = GEN / "build" / "assets" / rel.parent / rel.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        body = [f"#pragma once\n\nenum {stem}_offsets_e {{"]
        body += [f"    VS_{stem}_OFFSET_{k} = {i}," for i, k in enumerate(keys)]
        body += ["};\n", f"enum {stem}_indices_e {{"]
        body += [f"    VS_{stem}_INDEX_{k} = {i}," for i, k in enumerate(keys)]
        body += ["};"]
        Path(str(dest) + ".h").write_text("\n".join(body) + "\n", encoding="utf-8")
        Path(str(dest) + ".vsString").write_text("0\n", encoding="utf-8")
        count += 1
    return count


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--disc", default=str(REPO / "disks" / "SLUS-01040.cue"))
    a = ap.parse_args()
    n = write_lbas(Path(a.disc))
    s = write_strings()
    print(f"lbas.h: {n} files; string headers: {s} -> {GEN}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
