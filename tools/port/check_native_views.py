"""Check selected runtime views against canonical x64 layouts with libclang.

Run after copy_to_native.py. All non-padding view fields must reach the same original
field/byte in the native canonical record. This is separate from on-disc layout checks.
"""
from __future__ import annotations

import re
import sys

import clang.cindex as ci

import copy_to_native as ct
import native_views as nv
from layout_check import INCLUDES


def main() -> int:
    import json
    headers = sorted(p for p in json.loads(ct.MANIFEST.read_text()) if p.endswith('.h'))
    source = ''.join(f'#include "{ct.include_spelling(h)}"\n' for h in headers)
    args = ['-x', 'c', '-std=c17', '--target=x86_64-pc-windows-msvc', '-fms-extensions',
            '-fms-compatibility', '-Wno-everything'] + [f'-I{ct.NATIVE / p}' for p in INCLUDES]
    tu = ci.Index.create().parse('native_views.c', args=args, unsaved_files=[('native_views.c', source)])
    errors = [str(d) for d in tu.diagnostics if d.severity >= 3]
    if errors:
        print('\n'.join(errors[:20]))
        return 1
    cp = ct.Copier()
    cp.spelled_type('')
    sys.path.insert(0, str(ct.REPO / 'tools/naming/sym_gen'))
    from common import Gen
    gen = Gen('Shared')
    records = {c.spelling: c for c in tu.cursor.walk_preorder()
               if c.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL) and c.is_definition()}

    def native(tid):
        return records[ct.c_name(gen.types[tid]['final_name'])]

    def offsets(tid, base=0, native_base=0):
        record = native(tid)
        result = {}
        fields = {f.spelling: f for f in record.get_children() if f.kind == ci.CursorKind.FIELD_DECL}
        for member, field in zip(gen.members[tid], gen.layout(tid)['fields']):
            name = member['final_name']
            cursor = fields.get(name)
            if cursor is None:
                continue
            old, new = field['offset'], cursor.get_field_offsetof()
            result[base + old] = native_base + new
            if cursor.type.kind == ci.TypeKind.CONSTANTARRAY and cursor.type.element_type.kind == ci.TypeKind.POINTER:
                for index in range(cursor.type.element_count):
                    result[base + old + index * 32] = native_base + new + index * cursor.type.element_type.get_size() * 8
            spelling = field['type'].split('[')[0]
            sub = cp._tid_by_name.get(spelling)
            if sub and '*' not in spelling and sub != tid and ct.c_name(gen.types[sub]['final_name']) in records:
                count = int(re.search(r'\[(\d+)\]', field['type'])[1]) if '[' in field['type'] else 1
                native_type = cursor.type.element_type if count > 1 else cursor.type
                for i in range(count):
                    result.update(offsets(sub, base + old + i * int(gen.types[sub]['size']) * 8,
                                          native_base + new + i * native_type.get_size() * 8))
        # Generated padding was split to expose formerly hidden pointers.
        for name, cursor in fields.items():
            match = re.fullmatch(r'nativePointer([0-9A-F]+)|(?:reserved|padding)\w*_([0-9A-F]+)', name)
            if match:
                old = int(match[1] or match[2], 16) * 8
                result[base + old] = native_base + cursor.get_field_offsetof()
        return result

    failures = []
    checked = 0
    for family, canonical in ((nv.AI_VIEWS, 'T00532'), (nv.ACTOR_VIEWS, 'T00630'),
                              (nv.MODEL_VIEWS, 'T00239'), (nv.GIMMICK_VIEWS, 'T00255'),
                              (nv.GIM_VIEWS, 'T00542'), (nv.SPHERE_VIEWS, 'T00215')):
        mapping = offsets(canonical)
        for tid in sorted(family):
            if tid == canonical:
                continue
            view = native(tid)
            for member, field in zip(gen.members[tid], gen.layout(tid)['fields']):
                name = member['final_name']
                if name.startswith(('reserved', 'padding')):
                    continue
                old = field['offset']
                previous = max(o for o in mapping if o <= old)
                expected = mapping[previous] + old - previous
                actual = view.type.get_offset(name)
                checked += 1
                if actual != expected:
                    failures.append(f'{tid} {view.spelling}.{name}: PS1 bit {old:X}, native bit {actual:X}, expected {expected:X}')
    print(f'Checked {checked} runtime view fields; {len(failures)} mismatches')
    print('\n'.join(failures))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
