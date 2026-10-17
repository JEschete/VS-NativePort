"""Take ownership of a generated item, so it can be modernized by hand or by the rewrite tool.

  python tools\\port\\own.py <F#####|G#####|T#####> --to Native\\<Project>\\src\\<Area>\\<File>.c [--no-copy]

Moves the item's current generated text into the owned file (created if needed, never a generated file), with its
declaration in the owned file's sibling header, records it in tools\\port\\owned.csv, then reruns the copy so the
generated files stop emitting it and include the owned header where the name is used. From then on the copy tool
never regenerates that item; its ledger `modernized` column reads `wip` until the rewrite tool approves it (`x`).

Owned C files compile with the copied-C settings; promoting one to .cpp is a later, separate step (plan §8.2).
"""
from __future__ import annotations

import argparse
import csv
import datetime
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import copy_to_native as ct  # noqa: E402

FIELDS = ["id", "kind", "name", "path", "header", "since", "notes"]


def generated(cp: ct.Copier) -> dict[str, str]:
    cp.load_string_ids()
    cp.load_plugin_slots()
    cp.copy_types()
    cp.copy_globals()
    cp.copy_functions()
    cp.copy_templates()
    cp.copy_resets()
    cp.copy_disc_layout()
    cp.copy_string_tables()
    cp.copy_macros()
    return cp.finish()


def includes_of(text: str) -> list[str]:
    return [ln for ln in text.splitlines() if ln.startswith("#include ")]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("id")
    ap.add_argument("--to", required=True, help="owned .c file (repository-relative, Windows separators)")
    ap.add_argument("--no-copy", action="store_true", help="do not rerun copy_to_native.py afterwards")
    a = ap.parse_args()
    to = a.to.replace("/", "\\")
    if not to.startswith("Native\\") or not to.endswith(".c"):
        raise SystemExit("--to must be a Native\\...\\File.c path")
    header = to[:-2] + ".h"
    cp = ct.Copier()
    if a.id in cp.owned:
        raise SystemExit(f"{a.id} is already owned: {cp.owned[a.id]['path']}")
    rendered = generated(cp)
    if to in rendered or header in rendered:
        raise SystemExit(f"{to} (or its header) is a generated file; choose a new file for owned code")
    items = [(path, i) for path, f in cp.files.items() for i in f.items if i.row == a.id]
    body = [(p, i) for p, i in items if i.kind in ("function", "var", "type")]
    decl = [(p, i) for p, i in items if i.kind in ("proto", "extern")]
    if not body:
        raise SystemExit(f"{a.id}: no generated item (not copied, a dup-of row, or written inside another item)")
    src_path, item = body[0]
    kind = {"function": "function", "var": "global", "type": "type"}[item.kind]

    hdr_file, src_file = ct.REPO / header, ct.REPO / to
    guard = "VS_OWNED_" + "".join(ch if ch.isalnum() else "_" for ch in ct.include_spelling(header)).upper()
    if not hdr_file.exists():
        hdr_file.parent.mkdir(parents=True, exist_ok=True)
        hdr_file.write_text(f"#pragma once\n// Hand-maintained (tools/port/owned.csv).\n#ifndef {guard}\n#define {guard}\n"
                            f'#include "Legacy/Psx.h"\n\n#endif // {guard}\n', encoding="utf-8")
    h = hdr_file.read_text(encoding="utf-8")
    # what the declaration needs: the includes of the generated header it came from
    need = includes_of(rendered.get(decl[0][0], "")) if decl else includes_of(rendered.get(src_path, ""))
    add_inc = [ln for ln in need if ln not in h and ln != f'#include "{ct.include_spelling(header)}"']
    text = item.text if kind == "type" else "".join(i.text for _, i in decl)
    h = h.replace(f"\n#endif // {guard}", "".join(x + "\n" for x in add_inc) + "\n" + text + f"\n#endif // {guard}")
    hdr_file.write_text(h, encoding="utf-8")
    if kind != "type":
        if not src_file.exists():
            src_file.write_text(f"// Hand-maintained (tools/port/owned.csv).\n"
                                f'#include "{ct.include_spelling(header)}"\n', encoding="utf-8")
        s = src_file.read_text(encoding="utf-8")
        add_inc = [ln for ln in includes_of(rendered.get(src_path, "")) if ln not in s and
                   ln != f'#include "{ct.include_spelling(ct.sibling_header(src_path))}"']
        first_code = s.find("\n\n")
        head, rest = (s, "") if first_code < 0 else (s[:first_code + 1], s[first_code + 1:])
        s = head + "".join(x + "\n" for x in add_inc) + rest.rstrip("\n") + "\n\n" + item.text
        src_file.write_text(s, encoding="utf-8")

    rows = []
    if ct.OWNED.exists():
        with ct.OWNED.open(encoding="utf-8", newline="") as fh:
            rows = list(csv.DictReader(fh))
    rows.append({"id": a.id, "kind": kind, "name": item.name, "path": to if kind != "type" else header,
                 "header": header, "since": datetime.date.today().isoformat(),
                 "notes": f"from {src_path}"})
    with ct.OWNED.open("w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(rows)
    print(f"{a.id} {item.name}: owned in {rows[-1]['path']} (declared in {header})")
    if not a.no_copy:
        sys.argv = [sys.argv[0]]
        return ct.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
