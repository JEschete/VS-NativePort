"""Which field of a ledger type lies at a PS1 byte offset: for rewriting raw offsets (`(char*)actor + 0x5AC`)
in copied code as named fields, whose x64 offsets differ.

  python tools\\port\\field_at.py Actors_ActorRecord 0x5AC 0x14C
  python tools\\port\\field_at.py T00305 0x30

Prints, for each offset, the innermost named field (nested structs and arrays followed) as a C access path, e.g.
`.instance.flags` or `.palettes[3]` (+ remainder bytes if the offset falls inside a scalar).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import copy_to_native as ct  # noqa: E402


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cp = ct.Copier()
    want = sys.argv[1]
    by_c = {ct.c_name(r["final_name"]): r["id"] for r in cp.types}
    tid = want if want in cp.type_by_id else by_c.get(want)
    if tid is None:
        print(f"unknown type {want}")
        return 1
    cp.type_has_pointer(cp.canon_type(tid))  # builds cp._gen and the name tables
    for arg in sys.argv[2:]:
        off, _, bit = arg.partition(":")   # 0x5AC:24 = bit 24 of the word at 0x5AC
        print(f"{arg}: {path_at(cp, cp.canon_type(tid), int(off, 0), int(bit, 0) if bit else None)}")
    return 0


def path_at(cp: "ct.Copier", tid: str, off: int, bit: int | None = None) -> str:
    """Field path at byte `off` (and, inside a bitfield run, the field holding bit `bit` of the 32-bit word there)."""
    out = ""
    for _ in range(12):
        try:
            lay = cp._gen.layout(tid)
        except KeyError:
            return out + f" +{off} (no layout)"
        fields = list(zip(cp._gen.members[tid], lay["fields"]))
        total = int(lay.get("size") or 0) * 8
        want = off * 8 + (bit or 0)
        hit = None
        for i, (m, f) in enumerate(fields):
            start = f["offset"] or 0
            end = next((g["offset"] for _, g in fields[i + 1:] if (g["offset"] or 0) > start), total or start + 8)
            if f.get("bits"):
                end = start + f["bits"]
            if start <= want < end:
                hit = (m, f, start, end)
                break
        if hit is None:
            return out + f" +{off} (padding)"
        m, f, start, end = hit
        name = cp.fields[m["id"]]["final_name"] if m["id"] in cp.fields else f["name"]
        out += f".{name}"
        if f.get("bits"):
            return out + f"   (bitfield {f['bits']} bit(s) of {f['type']}, run bit {want - start} in field)"
        off -= start // 8
        size = (end - start) // 8
        t = f.get("type") or ""
        dims = [int(d) for d in re.findall(r"\[(\d+)\]", t)]
        base = re.sub(r"\[.*$", "", t).strip()
        elem = size
        if dims:
            n = 1
            for d in dims:
                n *= d
            elem = size // max(1, n)
            idx = off // elem if elem else 0
            out += f"[{idx}]"
            off -= idx * elem
        sub = cp._tid_by_name.get(base) if "*" not in base else None
        if sub and cp.type_by_id[sub]["kind"] in ("struct", "union"):
            tid = cp.canon_type(sub)
            continue
        return out + (f" +{off}" if off else "") + f"   ({t})"
    return out


if __name__ == "__main__":
    raise SystemExit(main())
