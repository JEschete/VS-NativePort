"""Find every access to an original RAM range in the disc executables: the evidence for retiring a write-only global
(Docs\\ledgers\\DATA_LEDGER.csv `retired-by:`; #347, #445).

Scans SLUS_010.40 and every .PRG and .BIN file under the extracted disc (Native\\data\\disc, see Docs\\BUILDING.md):
356 files. Each word is decoded as an R3000 instruction. An address built by `lui` is followed, through `addiu`,
`ori` and `addu`, until its register is overwritten or the function returns (`jr ra`), so both `lui`+offset and
base+offset forms are found, including indexed array bases. An aligned data word equal to an address in the range is
reported as a pointer. Hits in non-code files or in overlays that reuse the address for their own data are possible;
each is listed with its file offset for review. A range with only stores and no other hit is write-only.

  python tools\\naming\\disc_refs.py G01981 G01982        ranges of DATA_LEDGER rows (address and size)
  python tools\\naming\\disc_refs.py 0x8005FFB0:2           an address and size in bytes
"""
from __future__ import annotations

import argparse
import collections
import csv
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DISC = ROOT / "Native" / "data" / "disc"
LOADS = {0x20: "lb", 0x21: "lh", 0x22: "lwl", 0x23: "lw", 0x24: "lbu", 0x25: "lhu", 0x26: "lwr", 0x32: "lwc2"}
STORES = {0x28: "sb", 0x29: "sh", 0x2A: "swl", 0x2B: "sw", 0x2E: "swr", 0x3A: "swc2"}
WINDOW = 1024
RA = 31


def executables() -> list[Path]:
    files = [DISC / "SLUS_010.40"]
    files += sorted(path for path in DISC.rglob("*") if path.suffix.upper() in (".PRG", ".BIN") and path.is_file())
    return files


def destination(word: int) -> int | None:
    """The general register an instruction writes, if any."""
    op, rt, rd = word >> 26, (word >> 16) & 31, (word >> 11) & 31
    if op == 0:
        funct = word & 63
        if funct in (0x08, 0x0C, 0x0D, 0x18, 0x19, 0x1A, 0x1B, 0x11, 0x13):
            return None
        return rd
    if op == 3:
        return RA
    if op == 1:
        return RA if rt in (0x10, 0x11) else None
    if op in (0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F) or op in LOADS and op != 0x32:
        return rt
    if op in (0x10, 0x12) and ((word >> 21) & 31) in (0, 2):
        return rt
    return None


def scan(data: bytes, ranges: list[tuple[int, int]]) -> list[tuple[int, int, str, str]]:
    """(range index, file offset, kind, mnemonic): kind is read, write, address or pointer."""
    count = len(data) // 4
    words = struct.unpack_from(f"<{count}I", data)
    hits = []

    def hit(target: int, offset: int, kind: str, mnemonic: str, span: int = 1) -> None:
        for index, (low, size) in enumerate(ranges):
            if target < low + size and target + span > low:
                hits.append((index, offset, kind, mnemonic))

    for index, word in enumerate(words):
        if 0x80000000 <= word < 0x80200000:
            hit(word, index * 4, "pointer", ".word")
        if word >> 26 != 0x0F or (word >> 16) & 31 == 0:
            continue
        known = {(word >> 16) & 31: (word & 0xFFFF) << 16}
        for step in range(index + 1, min(index + WINDOW, count)):
            current = words[step]
            op, rs, rt = current >> 26, (current >> 21) & 31, (current >> 16) & 31
            immediate = current & 0xFFFF
            signed = immediate - 0x10000 if immediate & 0x8000 else immediate
            base = known.get(rs)
            if base is not None and (op in LOADS or op in STORES):
                width = {0x20: 1, 0x24: 1, 0x28: 1, 0x21: 2, 0x25: 2, 0x29: 2}.get(op, 4)
                hit((base + signed) & 0xFFFFFFFF, step * 4, "read" if op in LOADS else "write",
                    LOADS.get(op) or STORES[op], width)
            target = destination(current)
            value = None
            if base is not None and op in (0x09, 0x0D):
                value = (base + signed if op == 0x09 else base | immediate) & 0xFFFFFFFF
                hit(value, step * 4, "address", "addiu" if op == 0x09 else "ori")
            elif op == 0 and current & 63 in (0x21, 0x20) and (base is not None or rt in known):
                value = base if base is not None else known[rt]
            if target:
                known.pop(target, None)
                if value is not None:
                    known[target] = value
            if not known or (op == 0 and current & 63 == 0x08 and rs == RA):
                break
    return hits


def ranges_from(arguments: list[str]) -> list[tuple[str, int, int]]:
    rows = {}
    if any(not argument.startswith("0x") for argument in arguments):
        with (ROOT / "Docs" / "ledgers" / "DATA_LEDGER.csv").open(encoding="utf-8", newline="") as stream:
            rows = {row["id"]: row for row in csv.DictReader(stream)}
    found = []
    for argument in arguments:
        if argument.startswith("0x"):
            address, _, size = argument.partition(":")
            found.append((argument, int(address, 16), int(size or "1")))
        else:
            row = rows[argument]
            found.append((f"{argument} {row['final_name'] or row['name']}", int(row["address"], 16),
                          int(row["size"] or "1")))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("targets", nargs="+", help="DATA_LEDGER ids or 0xADDRESS[:SIZE]")
    args = parser.parse_args()
    if not DISC.is_dir():
        print(f"{DISC} is missing; extract the disc first (Docs\\BUILDING.md)")
        return 1
    targets = ranges_from(args.targets)
    spans = [(address, size) for _label, address, size in targets]
    found = collections.defaultdict(list)
    files = executables()
    for path in files:
        for index, offset, kind, mnemonic in scan(path.read_bytes(), spans):
            found[index].append((path.relative_to(DISC).as_posix(), offset, kind, mnemonic))
    print(f"{len(files)} executables")
    for index, (label, address, size) in enumerate(targets):
        kinds = collections.Counter(kind for *_rest, kind, _mnemonic in found[index])
        verdict = "write-only" if kinds and set(kinds) == {"write"} else "unreferenced" if not kinds else "used"
        print(f"{label} 0x{address:08X}+{size}: {verdict} {dict(kinds)}")
        for path, offset, kind, mnemonic in found[index]:
            print(f"  {path} +0x{offset:X} {kind} {mnemonic}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
