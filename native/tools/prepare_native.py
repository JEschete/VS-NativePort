"""Prepare host-only C inputs without modifying the authoritative decompilation."""

import argparse
from pathlib import Path
import shutil

from tree_sitter import Language, Parser
import tree_sitter_c

PARSER = Parser(Language(tree_sitter_c.language()))

FUNCTION_BODIES = {
    "_asmNop": b"{ }",
    "vs_main_memcpy": b"""{
    u_int count = sz & ~3u;
    u_int index;
    char* output = (char*)dest;
    const char* input = (const char*)src;
    for (index = 0; index < count; index++) output[index] = input[index];
}""",
    "vs_main_bzero": b"""{
    unsigned count = (unsigned)sz & 0xFFFCu;
    unsigned index;
    char* output = (char*)dest;
    for (index = 0; index < count; index++) output[index] = 0;
}""",
    "_renderAshley": b"{ (void)mode; }",
}

INSERT_TPAGE = b"""#pragma once
#include <psx_scratchpad.h>
static inline void _insertTPage(int after, int arg1)
{
    unsigned long* scratch = _psx_scratchpad();
    unsigned long* prim = (unsigned long*)scratch[0];
    unsigned long* entry = (unsigned long*)(scratch[1] + ((unsigned long)after << 2));
    unsigned long head = *entry;
    prim[1] = 0xE1000000u | 0x200u | ((unsigned long)arg1 & 0x1FFu);
    prim[2] = 0;
    prim[0] = 0x02000000u | (head & 0x00FFFFFFu);
    *entry = ((unsigned long)prim & 0x00FFFFFFu) | (head & 0xFF000000u);
    scratch[0] = (unsigned long)(prim + 3);
}
"""

INLINE_GTE = b"""
#include <psx_inline_gte.h>
#undef gte_SetRotMatrix
#undef gte_SetTransVector2
#undef gte_ldv0
#undef gte_rtv0tr2
#undef gte_rtps2
#undef gte_stlvnl
#undef gte_stsxy
#undef gte_stszotz
#undef gte_zrtr
#define gte_SetRotMatrix(matrix) psx_inline_rotation(matrix)
#define gte_SetTransVector2(vector) psx_inline_translation(vector)
#define gte_ldv0(vector) psx_inline_load_vector(vector)
#define gte_rtv0tr2() psx_inline_transform()
#define gte_rtps2() psx_inline_project()
#define gte_stlvnl(vector) psx_inline_store_vector(vector)
#define gte_stsxy(screen) psx_inline_store_screen(screen)
#define gte_stszotz(depth) psx_inline_store_depth(depth)
#define gte_zrtr() psx_inline_clear_translation()
"""


def replace_once(data: bytes, original: bytes, replacement: bytes) -> bytes:
    if data.count(original) != 1:
        raise ValueError("Upstream construct changed; review the native adapter")
    return data.replace(original, replacement, 1)


def declaration_name(node) -> str:
    while node is not None:
        if node.type in ("identifier", "type_identifier"):
            return node.text.decode("ascii")
        node = node.child_by_field_name("declarator")
    return ""


def transform_source(relative: Path, data: bytes, shared_type: bytes | None = None) -> bytes:
    path = relative.as_posix()
    if path in ("include/psx/inline_c.h", "src/include/vs_inline_c.h"):
        return data + INLINE_GTE
    if path == "src/MENU/MAINMENU.PRG/58EC.h":
        if not shared_type:
            raise ValueError("Upstream textHeader_t definition is required")
        return shared_type + b"\n" + data
    if path == "src/include/common.h":
        data = replace_once(data,
            b"#if !defined(PERMUTER) && !defined(OBJDIFF)",
            b"#if !defined(PERMUTER) && !defined(OBJDIFF) && !defined(NATIVE_BUILD)")
        return data + b"\n#undef BIOS_STUB\n#define BIOS_STUB(name, table, id)\n#undef SYSCALL\n#define SYSCALL(id)\n"
    if path == "src/include/insertTPage.h":
        return INSERT_TPAGE
    if path == "src/include/scratch.h":
        return data + b"\n#define vs_scratch (*(vs_scratch_t*)0x1F800000UL)\n"
    if path == "include/psx/libapi.h":
        for name in ("open", "close", "lseek", "read", "write"):
            tree = PARSER.parse(data)
            declarations = [node for node in tree.root_node.named_children
                if node.type == "declaration" and
                declaration_name(node.child_by_field_name("declarator")) == name]
            if not declarations:
                declarations = [node for node in walk_nodes(tree.root_node)
                    if node.type == "declaration" and
                    declaration_name(node.child_by_field_name("declarator")) == name]
            if len(declarations) != 1:
                raise ValueError(f"Upstream BIOS declaration changed: {name}")
            node = declarations[0]
            statement = node.text.rstrip().removesuffix(b";")
            replacement = statement + f' __asm__("_vs_bios_{name}");'.encode("ascii")
            data = data[:node.start_byte] + replacement + data[node.end_byte:]
        return data

    tree = PARSER.parse(data)
    edits = []
    effect_entry = None
    if path.startswith("src/EFFECT/"):
        effect_entry = ("vs_effect_" + relative.parent.name.replace(".", "_") + "_entry").encode("ascii")
    for node in walk_nodes(tree.root_node):
        if effect_entry and node.type == "identifier" and node.text == b"func_800F9800":
            edits.append((node.start_byte, node.end_byte, effect_entry))
        if path == "src/MENU/MAINMENU.PRG/58EC.c" and node.type == "type_definition":
            if declaration_name(node.child_by_field_name("declarator")) == "textHeader_t":
                edits.append((node.start_byte, node.end_byte, b""))
        if node.type == "expression_statement" and node.parent.type == "translation_unit":
            if any(child.type == "gnu_asm_expression" for child in node.named_children):
                edits.append((node.start_byte, node.end_byte, b""))
        if node.type != "function_definition":
            continue
        name = declaration_name(node.child_by_field_name("declarator"))
        if name in FUNCTION_BODIES:
            body = node.child_by_field_name("body")
            if name == "_renderAshley" or any(child.type == "gnu_asm_expression" for child in walk_nodes(body)):
                edits.append((body.start_byte, body.end_byte, FUNCTION_BODIES[name]))
        if name == "_packageGameSaveData":
            for descendant in walk_nodes(node.child_by_field_name("body")):
                if descendant.type == "expression_statement" and descendant.text.strip() == b"func_80042CB0();":
                    edits.append((descendant.start_byte, descendant.end_byte, b""))
    for start, finish, replacement in sorted(edits, reverse=True):
        data = data[:start] + replacement + data[finish:]
    return data


def walk_nodes(node):
    stack = [node]
    while stack:
        current = stack.pop()
        yield current
        stack.extend(reversed(current.named_children))


def prepare(source: Path, output: Path) -> None:
    if source.resolve() == output.resolve() or source.resolve() in output.resolve().parents:
        raise ValueError("Native output must be separate from the decompilation source")
    type_source = (source / "src/MENU/MAINMENU.PRG/58EC.c").read_bytes()
    shared_types = [node.text for node in PARSER.parse(type_source).root_node.named_children
        if node.type == "type_definition" and
        declaration_name(node.child_by_field_name("declarator")) == "textHeader_t"]
    if len(shared_types) != 1:
        raise ValueError("Upstream textHeader_t definition changed; review native declaration ordering")
    count = 0
    for folder in ("src", "include"):
        for original in sorted((source / folder).rglob("*")):
            if not original.is_file():
                continue
            relative = original.relative_to(source)
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if original.suffix in (".c", ".h"):
                content = transform_source(relative, original.read_bytes(), shared_types[0])
                if not target.exists() or target.read_bytes() != content:
                    target.write_bytes(content)
            else:
                shutil.copy2(original, target)
            count += 1
    print(f"Prepared {count} native input files from {source}")


if __name__ == "__main__":
    argument_parser = argparse.ArgumentParser(description=__doc__)
    argument_parser.add_argument("source", type=Path)
    argument_parser.add_argument("output", type=Path)
    arguments = argument_parser.parse_args()
    prepare(arguments.source, arguments.output)
