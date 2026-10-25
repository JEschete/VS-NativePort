"""Layout check: every generated struct/union compiled for x64 Windows (MSVC layout rules) against its PS1 layout.

Members are compared by bit offset (from the ledger layouts, which come from the decomp compiled for the PS1).
Types that hold pointers are expected to differ and are listed separately; a pointer-free type that differs is a
bug (its data is read from disc or exchanged as raw bytes), usually MSVC's bitfield packing.

  python tools\\port\\layout_check.py [--json out.json]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import clang.cindex as ci

sys.path.insert(0, str(Path(__file__).resolve().parent))
import copy_to_native as ct  # noqa: E402
import native_views as nv  # noqa: E402

K = ci.CursorKind
NATIVE = ct.REPO / "Native"
INCLUDES = ["Engine", "Content", "Engine/include", "Engine/src", "Content/include", "Content/src", "Game/include",
            "Game/src"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json")
    a = ap.parse_args()
    manifest = json.loads(ct.MANIFEST.read_text(encoding="utf-8"))
    headers = [p for p in manifest if p.endswith(".h")]
    src = "".join(f'#include "{ct.include_spelling(h)}"\n' for h in sorted(headers))
    args = ["-x", "c", "-std=c17", "--target=x86_64-pc-windows-msvc", "-fms-extensions", "-fms-compatibility",
            "-Wno-everything"] + [f"-I{NATIVE / i}" for i in INCLUDES]
    tu = ci.Index.create().parse("all_headers.c", args=args, unsaved_files=[("all_headers.c", src)])
    errors = [d.spelling for d in tu.diagnostics if d.severity >= 3]
    cp = ct.Copier()
    by_name = {ct.c_name(r["final_name"]): r for r in cp.types if cp.canon_type(r["id"]) == r["id"]}
    sys.path.insert(0, str(ct.REPO / "tools" / "naming" / "sym_gen"))
    from common import Gen
    gen = Gen("Shared")
    out = {"pointer-free": [], "with-pointers": [], "checked": 0, "parse_errors": errors[:20]}
    seen = set()
    for c in tu.cursor.walk_preorder():
        if c.kind not in (K.STRUCT_DECL, K.UNION_DECL) or not c.is_definition():
            continue
        name = c.spelling.split(" ")[-1]
        r = by_name.get(name)
        if r is None or name in seen:
            continue
        seen.add(name)
        try:
            lay = gen.layout(r["id"])
        except KeyError:
            continue
        fields = [f for f in c.get_children() if f.kind == K.FIELD_DECL]
        ps1 = [f["offset"] for f in lay["fields"]]
        if len(fields) != len(ps1):
            continue
        out["checked"] += 1
        diffs = []
        for f, want in zip(fields, ps1, strict=True):
            got = c.type.get_offset(f.spelling)
            if want is not None and want >= 0 and got != want:
                diffs.append({"member": f.spelling, "ps1_bit": want, "x64_bit": got,
                              "bitfield": f.is_bitfield(), "type": f.type.spelling})
        size_ps1, size_x64 = int(r["size"] or 0), c.type.get_size()
        if diffs or (size_ps1 and size_x64 != size_ps1):
            entry = {"type": name, "id": r["id"], "role": r["role"], "ps1_size": size_ps1, "x64_size": size_x64,
                     "first_diffs": diffs[:4], "bitfields": any(f.is_bitfield() for f in fields)}
            out["with-pointers" if cp.type_has_pointer(r["id"]) or nv.pointer_slots(r["id"]) else "pointer-free"].append(entry)
    print(f"checked {out['checked']} types; pointer-free mismatches {len(out['pointer-free'])}, "
          f"with pointers {len(out['with-pointers'])}; parse errors {len(errors)}")
    for e in out["pointer-free"]:
        d = e["first_diffs"][0] if e["first_diffs"] else {}
        print(f"  {e['type']} ({e['role']}) ps1 {e['ps1_size']} x64 {e['x64_size']} bitfields={e['bitfields']} "
              f"first: {d.get('member')} {d.get('ps1_bit')}->{d.get('x64_bit')}")
    if a.json:
        Path(a.json).write_text(json.dumps(out, indent=1), encoding="utf-8")
    return 1 if out["pointer-free"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
