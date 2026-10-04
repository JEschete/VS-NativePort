"""Generate named abort stubs from bounded Win32 COFF object metadata."""

import argparse
from pathlib import Path
import re
import struct

CRT_SYMBOLS = set("""abort abs atexit atoi calloc ceil close cos exit fclose fflush fgets
floor fopen fprintf fputc fread free fseek ftell fwrite getenv labs lseek malloc memcmp
memcpy memmove memset open perror printf puts qsort rand read realloc remove rename
rewind sin snprintf sprintf sqrt srand strcmp strcpy strlen strncmp strncpy strrchr
strtol strtoul time vfprintf vsnprintf vsprintf write setvbuf __security_cookie __security_check_cookie
__chkstk __chkstk_ms _fltused _except_handler4_common _aulldiv _aullrem _alldiv _allrem
_allmul _allshl _allshr _aullshr _ftol2 _ftol2_sse""".split())


def symbols(path: Path):
    content = path.read_bytes()
    if len(content) < 20:
        raise ValueError(f"Truncated COFF header: {path}")
    machine, sections, timestamp, offset, count, optional, flags = struct.unpack_from("<HHIIIHH", content)
    if machine != 0x14C:
        raise ValueError(f"Expected Win32 COFF object: {path}")
    strings = offset + count * 18
    if offset < 20 or strings + 4 > len(content):
        raise ValueError(f"Invalid COFF symbol bounds: {path}")
    string_size = struct.unpack_from("<I", content, strings)[0]
    if strings + string_size > len(content):
        raise ValueError(f"Invalid COFF string table: {path}")
    index = 0
    while index < count:
        position = offset + index * 18
        raw_name, value, section, kind, storage, auxiliary = struct.unpack_from("<8sIhHBB", content, position)
        if raw_name[:4] == b"\x00\x00\x00\x00":
            string_offset = struct.unpack_from("<I", raw_name, 4)[0]
            if string_offset < 4 or string_offset >= string_size:
                raise ValueError(f"Invalid COFF name offset: {path}")
            end = content.find(b"\x00", strings + string_offset, strings + string_size)
            if end < 0:
                raise ValueError(f"Unterminated COFF symbol: {path}")
            name = content[strings + string_offset:end].decode("ascii")
        else:
            name = raw_name.split(b"\x00", 1)[0].decode("ascii")
        if storage == 2:
            yield name, section, kind
        index += 1 + auxiliary
        if index > count:
            raise ValueError(f"Invalid COFF auxiliary count: {path}")


def generate(objects: Path, source: Path, output: Path):
    undefined, defined, functions = set(), set(), set()
    object_files = sorted(objects.rglob("*.obj"))
    if not object_files:
        raise ValueError("No native COFF objects found")
    for path in object_files:
        if "stubs" in path.stem:
            continue
        for name, section, kind in symbols(path):
            if section == 0:
                undefined.add(name)
            else:
                defined.add(name)
            if kind & 0x20:
                functions.add(name)
    for path in source.rglob("*.c"):
        for name in re.findall(r'INCLUDE_ASM\s*\([^,]+,\s*([A-Za-z_]\w*)\s*\)', path.read_text(encoding="utf-8")):
            functions.add("_" + name)
    missing = sorted(undefined - defined)
    lines = ['extern void _native_unimplemented(const char* name);']
    count_functions = count_data = 0
    for decorated in missing:
        if not decorated.startswith("_") or decorated.startswith("__imp_") or "@" in decorated:
            continue
        name = decorated[1:]
        if name in CRT_SYMBOLS:
            continue
        if decorated in functions:
            lines.append(f'void {name}(void) {{ _native_unimplemented("{name}"); }}')
            count_functions += 1
        else:
            lines.append(f'unsigned char {name}[4096] __attribute__((aligned(16)));')
            count_data += 1
    output.parent.mkdir(parents=True, exist_ok=True)
    generated = "\n".join(lines) + "\n"
    if not output.exists() or output.read_text(encoding="ascii") != generated:
        output.write_text(generated, encoding="ascii")
    print(f"Generated {count_functions} named function stubs and {count_data} data blocks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("objects", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    generate(arguments.objects, arguments.source, arguments.output)
