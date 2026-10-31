"""Derive the edits that make copied C compile as C++, from the compiler's own diagnostics.

  python tools\\port\\cpp_adapt.py derive      compile every source and fix what the rules below understand
  python tools\\port\\cpp_adapt.py report      print the remaining errors by kind
  python tools\\port\\cpp_adapt.py record      store the edits to generated files and verify their replay

The copier stays the source of generated files: this tool edits them in place, then records each changed line in
tools\\port\\cpp_adaptations.json as (path, old line, new line), which copy_to_native.py replays after it renders. Only
conversions that C performed implicitly are written out, so each edit states the conversion C already did. A
diagnostic no rule understands stays an error for a person to resolve.
"""
from __future__ import annotations

import collections
import difflib
import json
import os
import re
import runpy
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "lint"))
import quality  # noqa: E402

ADAPTATIONS = ROOT / "tools" / "port" / "cpp_adaptations.json"
WORKERS = int(os.environ.get("CPP_ADAPT_WORKERS", "12"))
DIAGNOSTIC = re.compile(
    r"^(?P<file>[A-Za-z]:.*?|[^(\n]+?)\((?P<line>\d+),(?P<col>\d+)\):?(?P<ranges>(?:\{\d+:\d+-\d+:\d+\})*)"
    r": (?P<level>error|note): (?P<message>.*)$",
    re.M,
)
RANGE = re.compile(r"\{(\d+):(\d+)-(\d+):(\d+)\}")
QUOTED = r"'((?:[^']|'(?=[^ ,:;)]))*)'(?: \(aka '[^']*'\))?"
CONVERSIONS = [
    re.compile(rf"^assigning to {QUOTED} from (?:incompatible type )?{QUOTED}"),
    re.compile(rf"^assigning to {QUOTED} from {QUOTED} converts between"),
    re.compile(rf"^cannot initialize a variable of type {QUOTED} with an? [lr]value of type {QUOTED}"),
    re.compile(rf"^cannot initialize an array element of type {QUOTED} with an? [lr]value of type {QUOTED}"),
    re.compile(rf"^cannot initialize a member subobject of type {QUOTED} with an? [lr]value of type {QUOTED}"),
    re.compile(rf"^cannot initialize return object of type {QUOTED} with an? [lr]value of type {QUOTED}"),
    re.compile(rf"^cannot initialize a parameter of type {QUOTED} with an? [lr]value of type {QUOTED}"),
    re.compile(rf"^incompatible pointer types assigning to {QUOTED} from {QUOTED}"),
]
NARROWED_VALUE = re.compile(rf"^non-constant-expression cannot be narrowed from type {QUOTED} to {QUOTED}")
UNPROTOTYPED = re.compile(r"\(\*(\s*\w*)\)\(\s*\)")
NARROWED = re.compile(rf"^constant expression evaluates to (-?\d+) which cannot be narrowed to type {QUOTED}")
NOT_VIABLE = re.compile(
    rf"(?:no known conversion from|cannot convert argument of incomplete type) {QUOTED} to {QUOTED} for (\d+)(?:st|nd|rd|th) argument"
)
INTEGER_BITS = {"signed char": 8, "char": 8, "unsigned char": 8, "u_char": 8, "short": 16, "u_short": 16,
                "unsigned short": 16, "int": 32, "unsigned int": 32, "u_int": 32, "long": 32, "u_long": 32}
SIGNED = {"signed char", "short", "int", "long"}


def sources() -> list[str]:
    listed = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "Native"], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.splitlines()
    return [path for path in listed if path.endswith((".cpp", ".c")) and (ROOT / path).is_file()]


def compile_arguments() -> callable:
    compiler = quality.llvm_tool("clang-cl")
    scaffold = runpy.run_path(str(ROOT / "tools/scaffold/gen_solution.py"))
    properties = ET.parse(ROOT / "Directory.Build.props").getroot()
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    native = ROOT / "Native"
    copied = [value.replace("$(VSNative)", str(native) + os.sep)
              for value in properties.findtext(".//m:VSCopiedCIncludes", namespaces=namespace).split(";")]

    def arguments(path: str) -> list[str]:
        source = ROOT / path
        project = next((p for p in scaffold["PROJECTS"] if source.is_relative_to(p.dir)), None)
        includes = [ROOT / "vcpkg_installed/x64-windows/include", ROOT / "vcpkg_installed/x64-windows/include/webm"]
        if project:
            includes += [project.dir / "include", project.dir / "src"]
            includes += [scaffold["BY_NAME"][name].dir / "include" for name in project.refs + project.see]
        includes += copied
        return [compiler, "/nologo", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX", "/D_CRT_SECURE_NO_WARNINGS", "/TP",
                "/std:c++latest", "/EHsc", "/J", "/clang:-fsyntax-only", "/clang:-ferror-limit=0", "/clang:-w",
                "/clang:-fms-extensions", "/clang:-fdiagnostics-print-source-range-info",
                *[f"/I{directory}" for directory in includes], str(source)]
    return arguments


def compile_all(paths: list[str]) -> list[tuple[str, str]]:
    arguments = compile_arguments()

    def one(path: str) -> tuple[str, str]:
        result = subprocess.run(arguments(path), cwd=ROOT, capture_output=True, text=True, errors="replace")
        return path, result.stderr
    with ThreadPoolExecutor(WORKERS) as pool:
        return list(pool.map(one, paths))


def relative(file: str) -> str | None:
    try:
        return Path(os.path.normpath(ROOT / file)).relative_to(ROOT).as_posix()
    except ValueError:
        return None


def split_arguments(text: str, opening: int) -> list[tuple[int, int]]:
    """Offsets of each top-level argument in the call whose `(` is at `opening`."""
    spans, depth, start, index = [], 0, opening + 1, opening
    quote = None
    while index < len(text):
        char = text[index]
        if quote:
            if char == "\\":
                index += 1
            elif char == quote:
                quote = None
        elif char in "\"'":
            quote = char
        elif char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if depth == 0:
                spans.append((start, index))
                return spans
        elif char == "," and depth == 1:
            spans.append((start, index))
            start = index + 1
        index += 1
    raise ValueError("unterminated call")


class File:
    def __init__(self, path: str) -> None:
        self.path = path
        self.raw = (ROOT / path).read_bytes().decode("utf-8", errors="surrogateescape")
        self.lines = self.raw.split("\n")
        self.offsets = [0]
        for line in self.lines:
            self.offsets.append(self.offsets[-1] + len(line) + 1)
        self.edits: dict[tuple[int, int], tuple[int, str]] = {}

    def offset(self, line: int, column: int) -> int:
        return self.offsets[line - 1] + column - 1

    def replace(self, start: int, end: int, text: str) -> None:
        self.edits.setdefault((start, end), (start, text))

    def apply(self) -> int:
        ordered = sorted(self.edits, reverse=True)
        applied, floor = 0, len(self.raw) + 1
        text = self.raw
        for start, end in ordered:
            if end > floor:  # overlaps a later edit: resolved on the next pass
                continue
            text = text[:start] + self.edits[(start, end)][1] + text[end:]
            floor = start
            applied += 1
        if applied:
            (ROOT / self.path).write_bytes(text.encode("utf-8", errors="surrogateescape"))
        return applied


def cast_text(target: str, expression: str) -> str:
    if "(*" not in target:
        target = re.sub(r"\[\d*\]$", "*", target)
    if "(*" in target or target.endswith("*") or re.fullmatch(r"[\w ]+", target):
        return f"({target})({expression.strip()})"
    raise ValueError(target)


def fix_conversion(file: File, target: str, source: str, ranges: list[tuple[int, int, int, int]]) -> bool:
    if not ranges:
        return False
    line, column, end_line, end_column = ranges[-1]
    start, end = file.offset(line, column), file.offset(end_line, end_column)
    expression = file.raw[start:end]
    if "\n" in expression and "{" in expression:
        return False
    if "unnamed" in target or "anonymous" in target:
        assigned = re.search(r"(\w+)\s*=\s*$", file.raw[file.offsets[line - 1]:start])
        if not assigned:
            return False
        file.replace(start, end, f"(decltype({assigned[1]}))({expression.strip()})")
        return True
    if "unnamed" in source:
        return False
    if expression.startswith(f"({target})"):
        return False
    try:
        file.replace(start, end, cast_text(target, expression))
    except ValueError:
        return False
    return True


def fix_narrowing(file: File, value: int, target: str, ranges: list[tuple[int, int, int, int]]) -> bool:
    if not ranges or target not in INTEGER_BITS:
        return False
    line, column, end_line, end_column = ranges[-1]
    start, end = file.offset(line, column), file.offset(end_line, end_column)
    expression = file.raw[start:end]
    if re.fullmatch(r"\d+|0[xX][0-9a-fA-F]+", expression):
        bits = INTEGER_BITS[target]
        wrapped = value & ((1 << bits) - 1)
        if target in SIGNED and wrapped >= 1 << (bits - 1):
            wrapped -= 1 << bits
        file.replace(start, end, str(wrapped))
    else:
        file.replace(start, end, f"({target})({expression})")
    return True


def fix_call_argument(file: File, line: int, column: int, target: str, position: int) -> bool:
    start = file.offset(line, column)
    match = re.compile(r"\s*\w*\s*\(").match(file.raw, start)
    if not match:
        return False
    opening = match.end() - 1
    try:
        spans = split_arguments(file.raw, opening)
    except ValueError:
        return False
    if position > len(spans):
        return False
    first, last = spans[position - 1]
    expression = file.raw[first:last]
    leading = expression[: len(expression) - len(expression.lstrip())]
    try:
        file.replace(first, last, leading + cast_text(target, expression))
    except ValueError:
        return False
    return True


def fix_void_arithmetic(file: File, column: int, ranges: list[tuple[int, int, int, int]]) -> bool:
    """GNU C counts void* arithmetic in bytes, which a byte pointer states directly."""
    if not ranges:
        return False
    line, first, end_line, end_column = ranges[0]
    if column < end_column:  # the range is a macro use, not the pointer: a person writes this one out
        return False
    start, end = file.offset(line, first), file.offset(end_line, end_column)
    pointer = file.raw[start:end]
    following = file.raw[end:end + 4].lstrip()
    if following.startswith(("++", "--")) or file.raw[max(0, start - 2):start] in ("++", "--"):
        return False
    if following.startswith(("+=", "-=")):
        operator = file.raw.index(following[:2], end)
        depth, index = 0, operator + 2
        while index < len(file.raw):
            char = file.raw[index]
            if char in "([{":
                depth += 1
            elif char in ")]}":
                if depth == 0:
                    break
                depth -= 1
            elif char in ";," and depth == 0:
                break
            index += 1
        amount = file.raw[operator + 2:index].strip()
        file.replace(start, index, f"{pointer} = (u_char*)({pointer}) {following[0]} ({amount})")
        return True
    file.replace(start, end, f"((u_char*)({pointer}))")
    return True


def fix_enum_operand(file: File, ranges: list[tuple[int, int, int, int]]) -> bool:
    """C treated both enumerators as plain int, which C++ asks to have stated."""
    if not ranges:
        return False
    line, column, end_line, end_column = ranges[0]
    start, end = file.offset(line, column), file.offset(end_line, end_column)
    if file.raw[start:end].startswith("(int)"):
        return False
    file.replace(start, end, f"(int){file.raw[start:end]}")
    return True


def prototype_pass(paths: list[str]) -> int:
    """`T (*f)()` took any arguments in C and none in C++; `(...)` keeps the C call on this ABI."""
    changed = 0
    headers = {
        path.replace(chr(92), "/")
        for path in json.loads((ROOT / "tools/port/copy_manifest.json").read_text(encoding="utf-8"))
    }
    listed = set(headers)
    for path in (ROOT / "tools/port/relaxed_sources.txt").read_text(encoding="utf-8").split():
        posix = path.replace(chr(92), "/")
        listed.add(posix)
        listed.add(posix[:-4] + ".h")
    for name in sorted(listed):
        target = ROOT / name
        if not target.is_file():
            continue
        raw = target.read_bytes().decode("utf-8", errors="surrogateescape")
        new = UNPROTOTYPED.sub(lambda m: f"(*{m[1]})(...)", raw)
        if new != raw:
            target.write_bytes(new.encode("utf-8", errors="surrogateescape"))
            changed += 1
    return changed


def fix_empty_return(file: File, line: int, column: int) -> bool:
    """C let a non-void function return nothing when no caller read the value."""
    start = file.offset(line, column)
    match = re.compile(r"return\s*;").match(file.raw, start)
    if not match:
        return False
    file.replace(start, match.end(), "return 0;")
    return True


def fix_unsized_array(file: File, line: int, column: int) -> bool:
    """A C tentative definition of `T x[]` has exactly one element."""
    start = file.offset(line, column)
    match = re.compile(r"\w*\s*\[\s*\]").match(file.raw, start)
    if not match:
        return False
    file.replace(match.end() - 1, match.end() - 1, "1")
    return True


def fix_const_default(file: File, line: int, column: int) -> bool:
    """C zero-initializes a file-scope tentative const definition."""
    start = file.offset(line, column)
    end = file.raw.find(";", start)
    if end < 0 or "\n" in file.raw[start:end]:
        return False
    file.replace(end, end, " = {}")
    return True


def derive_pass(paths: list[str]) -> tuple[int, int]:
    results = compile_all(paths)
    files: dict[str, File] = {}
    seen, fixed = set(), 0
    remaining = 0

    def get(path: str) -> File:
        return files.setdefault(path, File(path))

    for _path, stderr in results:
        items = list(DIAGNOSTIC.finditer(stderr))
        for index, item in enumerate(items):
            if item["level"] != "error":
                continue
            where = relative(item["file"])
            key = (where, item["line"], item["col"], item["message"])
            if key in seen:
                continue
            seen.add(key)
            if where is None or not where.startswith("Native/"):
                remaining += 1
                continue
            message = item["message"]
            ranges = [tuple(map(int, r)) for r in RANGE.findall(item["ranges"])]
            done = False
            for pattern in CONVERSIONS:
                match = pattern.match(message)
                if match:
                    done = fix_conversion(get(where), match[1], match[2], ranges)
                    break
            else:
                match = NARROWED.match(message)
                if match:
                    done = fix_narrowing(get(where), int(match[1]), match[2], ranges)
                elif message == "arithmetic on a pointer to void":
                    done = fix_void_arithmetic(get(where), int(item["col"]), ranges)
                elif NARROWED_VALUE.match(message):
                    done = fix_conversion(get(where), NARROWED_VALUE.match(message)[2], NARROWED_VALUE.match(message)[1], ranges)
                elif "between different enumeration types" in message:
                    done = fix_enum_operand(get(where), ranges)
                elif "should return a value" in message:
                    done = fix_empty_return(get(where), int(item["line"]), int(item["col"]))
                elif "array type needs an explicit size" in message:
                    done = fix_unsized_array(get(where), int(item["line"]), int(item["col"]))
                elif message.startswith("default initialization of an object of const type"):
                    done = fix_const_default(get(where), int(item["line"]), int(item["col"]))
                elif message.startswith("no matching function for call"):
                    notes = []
                    for note in items[index + 1:]:
                        if note["level"] != "note":
                            break
                        notes.append(note["message"])
                    viable = [NOT_VIABLE.search(note) for note in notes]
                    if len(viable) == 1 and viable[0]:
                        done = fix_call_argument(get(where), int(item["line"]), int(item["col"]), viable[0][2],
                                                 int(viable[0][3]))
            fixed += done
            remaining += not done
    applied = sum(file.apply() for file in files.values())
    return applied, remaining


def report(paths: list[str]) -> int:
    kinds, samples, seen = collections.Counter(), {}, set()
    for _path, stderr in compile_all(paths):
        for item in DIAGNOSTIC.finditer(stderr):
            if item["level"] != "error":
                continue
            key = (item["file"], item["line"], item["col"], item["message"])
            if key in seen:
                continue
            seen.add(key)
            kind = re.sub(r"\d+", "N", re.sub(r"'[^']*'", "'_'", item["message"]))
            kinds[kind] += 1
            samples.setdefault(kind, f"{relative(item['file'])}({item['line']}): {item['message']}")
    print(f"{len(seen)} errors")
    for kind, count in kinds.most_common(60):
        print(f"{count:6} {samples[kind][:210]}")
    return 1 if seen else 0


def record() -> int:
    """Store how the generated files differ from the copy tool's raw output, and prove the replay reproduces them."""
    sys.path.insert(0, str(ROOT / "tools" / "port"))
    import copy_to_native as ct

    raw = ct.render(ct.Copier())
    table: dict[str, list] = {}
    for path, text in sorted(raw.items()):
        disk = (ROOT / path).read_bytes().decode("utf-8", errors="surrogateescape")
        if disk == text:
            continue
        old, new = text.split("\n"), disk.split("\n")
        hunks = []
        for tag, a, b, c, d in difflib.SequenceMatcher(None, old, new, autojunk=False).get_opcodes():
            if tag == "equal":
                continue
            if a == b:  # an insertion anchors on the line before it
                a, c = a - 1, c - 1
            hunks.append([a, old[a:b], new[c:d]])
        table[path] = hunks
    ADAPTATIONS.write_text(json.dumps(table, indent=0, sort_keys=True) + "\n", encoding="utf-8", errors="surrogateescape")
    replayed = ct.apply_cpp_adaptations(raw)
    wrong = [p for p, text in replayed.items() if (ROOT / p).read_bytes().decode("utf-8", errors="surrogateescape") != text]
    if wrong:
        print("replay differs:", wrong[:5])
        return 1
    manifest = {p: ct.content_hash(text.encode("utf-8", errors="surrogateescape")) for p, text in sorted(replayed.items())}
    ct.MANIFEST.write_text(json.dumps(manifest, indent=0, sort_keys=True) + "\n", encoding="utf-8")
    print(f"{len(table)} files adapted, {sum(len(h) for h in table.values())} hunks")
    return 0


def main() -> int:
    command = sys.argv[1] if len(sys.argv) > 1 else "report"
    if command == "record":
        return record()
    paths = sources()
    if command == "report":
        return report(paths)
    print(f"prototypes: {prototype_pass(paths)} files", flush=True)
    for attempt in range(12):
        applied, remaining = derive_pass(paths)
        print(f"pass {attempt}: applied {applied}, not understood {remaining}", flush=True)
        if not applied:
            break
    return report(paths)


if __name__ == "__main__":
    raise SystemExit(main())
