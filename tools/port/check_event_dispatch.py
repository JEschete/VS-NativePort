"""Audit the native event opcode ABI and the normal/skip table target set.

Run after copy_to_native.py. Requires libclang and extracted Native/data/disc/BATTLE/BATTLE.PRG;
--module can select another copy of the same US module. No module contents are written to the repo.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import struct
from pathlib import Path

import clang.cindex as ci
import copy_fixups as cf
import copy_to_native as ct
from layout_check import CXX_ARGS, INCLUDES, c_includes, use_toolchain_libclang


def top_level(cursor: ci.Cursor):
    """Namespace-scope declarations, including those inside extern "C" blocks."""
    for child in cursor.get_children():
        if child.kind == ci.CursorKind.LINKAGE_SPEC:
            yield from top_level(child)
        else:
            yield child


def opcode_symbol(fid: str, final_name: str) -> str:
    """The C symbol in an opcode table slot: a promoted handler's binding, otherwise the catalog name."""
    return cf.EVENT_OPCODE_BINDINGS.get(fid) or ct.c_name(final_name)


def event_headers(manifest: dict[str, str],
                  owned: dict[str, dict[str, str]]) -> list[str]:
    """Include C opcode declarations (promoted handlers by their binding) without unrelated C++ headers."""
    headers = {path for path in manifest if path.endswith(".h")}
    for identifier, row in owned.items():
        if identifier in cf.EVENT_OPCODE_HANDLERS:
            binding = cf.EVENT_OPCODE_BINDINGS.get(identifier)
            header = cf.NATIVE_HELPER_HEADERS.get(binding, "") if binding else row.get("header", "").strip()
            if not header:
                raise ValueError(f"{identifier}: owned opcode handler needs a header")
            headers.add(header)
    return sorted(headers)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--module', type=Path, default=ct.NATIVE / 'data/disc/BATTLE/BATTLE.PRG')
    args = ap.parse_args()
    use_toolchain_libclang()
    cp = ct.Copier()
    headers = event_headers(json.loads(ct.MANIFEST.read_text()), cp.owned)
    source = c_includes(headers)
    flags = CXX_ARGS + [f'-I{ct.NATIVE / p}' for p in INCLUDES]
    tu = ci.Index.create().parse('event_contract.cpp', args=flags, unsaved_files=[('event_contract.cpp', source)])
    errors = [str(d) for d in tu.diagnostics if d.severity >= 3]
    if errors:
        print('\n'.join(errors))
        return 1
    declarations = list(top_level(tu.cursor))
    functions = {c.spelling: c for c in declarations if c.kind == ci.CursorKind.FUNCTION_DECL}
    variables = {c.spelling: c for c in declarations if c.kind == ci.CursorKind.VAR_DECL}
    expected = 'long long (unsigned char *, short)'  # Windows x64 intptr_t, bytecode pointer, thread index
    names = {opcode_symbol(fid, cp.cat_by_id[fid]['final_name']) for fid in cf.EVENT_OPCODE_HANDLERS}
    for name in sorted(names):
        f = functions.get(name)
        if f is None or f.type.get_canonical().spelling != expected:
            errors.append(f'{name}: expected {expected}; got {f.type.spelling if f else "missing"}')
    for name in ('Event_kOpcodeHandlers', 'Event_kSkipOpcodeHandlers', 'Event_legacySkipHandlerCopy'):
        t = variables[name].type
        element = t.get_pointee() if t.kind == ci.TypeKind.POINTER else t.element_type
        if element.get_pointee().get_canonical().spelling != expected:
            errors.append(f'{name}: incompatible dispatch type {t.spelling}')
    for name in ('Event_kOpcodeHandlers', 'Event_kSkipOpcodeHandlers'):
        count = variables[name].type.element_count
        if count != 256:
            errors.append(f'{name}: expected 256 native pointers, got {count}')

    # G00133 is loaded from disc. Audit the 256 byte-indexed entries, not the ledger's overestimated data span.
    with (cp.SPLAT_ARTIFACTS / 'BATTLE/BATTLE.PRG/splat_symbols.csv').open(newline='') as fh:
        symbols = {int(r['vram_start'], 16): r['name'] for r in csv.DictReader(fh) if r['type'] == 'func'}
    by_original = {r['name']: opcode_symbol(r['id'], r['final_name']) for r in cp.cat if r['overlay'] == 'BATTLE.PRG'}
    module = args.module.read_bytes()
    offset = int(cp.data_by_id['G00133']['file_offset'], 0)
    normal = set()
    for opcode, address in enumerate(struct.unpack_from('<256I', module, offset)):
        name = by_original.get(symbols.get(address))
        if name is None:
            errors.append(f'normal opcode {opcode:02X}: unresolved address {address:08X}')
        else:
            normal.add(name)
    vm = (ct.NATIVE / 'Game/src/Event/ScriptVm.cpp').read_text()
    skip_match = re.search(r'Event_kSkipOpcodeHandlers\[256\][^=]*=\s*\{([^}]+)\}', vm)
    if skip_match is None:
        errors.append('skip table initializer not found')
    else:
        skip = re.findall(r'\bEvent_\w+\b', skip_match[1])
        if len(skip) != 256:
            errors.append(f'skip table initializer has {len(skip)} entries, expected 256')
        actual = normal | set(skip)
        if actual != names:
            errors.append(f'handler set differs: unreviewed={sorted(actual - names)}, unused={sorted(names - actual)}')
    print(f'Checked {len(names)} event handler signatures and normal/skip table targets; {len(errors)} errors')
    if errors:
        print('\n'.join(errors))
    return int(bool(errors))


if __name__ == '__main__':
    raise SystemExit(main())
