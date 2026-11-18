"""Type-aware member and static-variable references in the native tree (#445).

Parses every Native .cpp translation unit with libclang, using the compile commands of the quality tool and the
libclang that ships beside its clang-cl, and records:

  decls  USR -> kind (field or var), qualified record, name, declaration path:line, native bit offset and width,
         native record size in bytes, type spelling; for a member of an anonymous struct or union also the `owner`
         record it belongs to and its `owner_offset`/`owner_size` there. Fields also carry `offset32`/`record_size32`
         (and `owner_offset32`/`owner_size32`): the same declarations laid out by the original's rules (32-bit
         pointers, GCC bitfield packing, 8-byte alignment of 64-bit types), which gives the original offsets
         wherever native code kept the original layout
  refs   USR -> [path, line, function, access, peer]
  loose  member spellings referenced through unresolved (dependent) expressions -> first path:line

`access` is one of:
  read     the value is used
  copy     the whole right-hand side of an assignment or designated initializer; `peer` is the target USR
  write    plain assignment target, designated or constructor initializer; `peer` is the copied source USR, if any
  update   compound assignment, increment or decrement
  address  address taken, array or reference argument, or offsetof outside a static_assert
  layout   sizeof, alignof, decltype or a static_assert

A value read inside a larger right-hand side keeps `read` with the assignment target as `peer`, so a member copied
into a named native mirror through a conversion can be found. Raw byte-offset accesses are not visible here.

  python tools\\naming\\native_refs.py [--out build\\naming\\native_refs.json] [--jobs N]
"""
from __future__ import annotations

import argparse
import ctypes
import functools
import json
import multiprocessing
import multiprocessing.pool
import os
import subprocess
import sys
from pathlib import Path

import clang.cindex as ci

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "Native"
OUT = ROOT / "build" / "naming" / "native_refs.json"
K = ci.CursorKind
FUNCTIONS = {"FUNCTION_DECL", "CXX_METHOD", "CONSTRUCTOR", "DESTRUCTOR", "FUNCTION_TEMPLATE",
             "CONVERSION_FUNCTION"}
SCOPES = {"NAMESPACE", "STRUCT_DECL", "UNION_DECL", "CLASS_DECL", "CLASS_TEMPLATE"} | FUNCTIONS
PASS_THROUGH = {"UNEXPOSED_EXPR", "PAREN_EXPR", "CSTYLE_CAST_EXPR", "CXX_STATIC_CAST_EXPR",
                "CXX_FUNCTIONAL_CAST_EXPR", "CXX_REINTERPRET_CAST_EXPR", "CXX_CONST_CAST_EXPR"}
ASSIGN, FIRST_COMPOUND, LAST_COMPOUND = 22, 23, 32
HEADERS = {".h", ".hpp", ".inl"}
ADDRESS_OF, LAST_INCREMENT = 5, 4


def libclang_path() -> Path:
    sys.path.insert(0, str(ROOT / "tools" / "lint"))
    import quality  # noqa: PLC0415

    return Path(quality.llvm_tool("clang-cl")).with_name("libclang.dll")


def configure(library: Path) -> None:
    if not ci.Config.loaded:
        ci.Config.set_library_file(str(library))
    lib = ci.conf.lib
    for name in ("clang_getCursorBinaryOperatorKind", "clang_getCursorUnaryOperatorKind",
                 "clang_Cursor_isAnonymousRecordDecl"):
        getattr(lib, name).argtypes = [ci.Cursor]
        getattr(lib, name).restype = ctypes.c_int


def kind(cursor: ci.Cursor) -> str:
    try:
        return cursor.kind.name
    except ValueError:
        return "UNKNOWN"


NATIVE_PREFIX = os.path.normcase(str(NATIVE)) + os.sep


@functools.cache
def relative(name: str) -> str | None:
    """Repository-relative path of a file under Native, else None."""
    full = os.path.normpath(name)
    if not os.path.normcase(full).startswith(NATIVE_PREFIX):
        return None
    return "Native\\" + full[len(NATIVE_PREFIX):]


def qualified(cursor: ci.Cursor | None) -> str:
    """Qualified name; an unnamed struct or union is `(anonymous:<line>)` so sibling unnamed records stay apart."""
    parts = []
    while cursor is not None and kind(cursor) != "TRANSLATION_UNIT":
        if kind(cursor) in SCOPES:
            name = cursor.spelling
            if not name or name.startswith("(unnamed") or name.startswith("(anonymous"):
                name = "(anonymous)" if kind(cursor) == "NAMESPACE" else f"(anonymous:{cursor.location.line})"
            parts.append(name)
        cursor = cursor.semantic_parent
    return "::".join(reversed(parts))


def strip(cursor: ci.Cursor) -> ci.Cursor:
    while kind(cursor) in PASS_THROUGH:
        children = [child for child in cursor.get_children() if kind(child) != "TYPE_REF"]
        if len(children) != 1:
            break
        cursor = children[0]
    return cursor


RECORDS = {"STRUCT_DECL", "UNION_DECL", "CLASS_DECL"}


def anonymous_member(record: ci.Cursor | None) -> bool:
    """An anonymous struct or union whose members belong to the enclosing record."""
    return record is not None and kind(record) in RECORDS and record.semantic_parent is not None and \
        kind(record.semantic_parent) in RECORDS and bool(ci.conf.lib.clang_Cursor_isAnonymousRecordDecl(record))


def placement(field: ci.Cursor) -> tuple[int | None, int | None, ci.Cursor | None, int | None, int | None]:
    """Bit offset of a field in its record and that record's size; for a member of an anonymous struct or union,
    also the record it belongs to through anonymous members, with the offset and size there."""
    def known(value: int) -> int | None:
        return value if value >= 0 else None

    record = field.semantic_parent
    offset, size = known(field.get_field_offsetof()), known(record.type.get_size())
    owner = record
    while anonymous_member(owner):
        owner = owner.semantic_parent
    if owner == record:
        return offset, size, None, None, None
    return offset, size, owner, known(owner.type.get_offset(field.spelling)), known(owner.type.get_size())


class Walker:
    def __init__(self) -> None:
        self.decls: dict[str, dict] = {}
        self.refs: set[tuple] = set()
        self.loose: dict[str, str] = {}
        self.seen: set[tuple] = set()
        self.included: set[str] = set()
        self.lib = ci.conf.lib

    def tracked(self, cursor: ci.Cursor | None) -> str | None:
        if cursor is None:
            return None
        decl_kind = kind(cursor)
        if decl_kind == "VAR_DECL":
            parent = kind(cursor.semantic_parent) if cursor.semantic_parent else ""
            if parent not in ("TRANSLATION_UNIT", "NAMESPACE", "LINKAGE_SPEC") and \
                    cursor.storage_class != ci.StorageClass.STATIC:
                return None
        elif decl_kind != "FIELD_DECL":
            return None
        if cursor.location.file is None or relative(cursor.location.file.name) is None:
            return None
        usr = cursor.get_usr()
        if usr and usr not in self.decls:
            entry = {"kind": "field" if decl_kind == "FIELD_DECL" else "var", "name": cursor.spelling,
                     "path": relative(cursor.location.file.name), "line": cursor.location.line,
                     "type": cursor.type.spelling}
            if decl_kind == "FIELD_DECL":
                offset, size, owner, owner_offset, owner_size = placement(cursor)
                entry["record"] = qualified(cursor.semantic_parent)
                entry["offset"] = offset
                entry["bits"] = cursor.get_bitfield_width() if cursor.is_bitfield() else None
                entry["record_size"] = size
                if owner is not None:
                    entry["owner"] = qualified(owner)
                    entry["owner_offset"], entry["owner_size"] = owner_offset, owner_size
            else:
                entry["record"] = qualified(cursor.semantic_parent)
            self.decls[usr] = entry
        return usr

    def target(self, cursor: ci.Cursor) -> str | None:
        cursor = strip(cursor)
        if kind(cursor) in ("MEMBER_REF_EXPR", "DECL_REF_EXPR", "MEMBER_REF"):
            return self.tracked(cursor.referenced)
        return None

    def record(self, cursor: ci.Cursor, usr: str, function: str, access: str, peer: str | None) -> None:
        path = relative(cursor.location.file.name) if cursor.location.file else None
        if path:
            self.refs.add((usr, path, cursor.location.line, function, access, peer or ""))

    def visit(self, cursor: ci.Cursor, function: str, access: str = "read", peer: str | None = None) -> None:
        """`peer` is the copied source for a write, else the assignment target the value flows into."""
        k = kind(cursor)
        if k in FUNCTIONS and cursor.is_definition():
            function = qualified(cursor)
        if k in ("STATIC_ASSERT", "CXX_UNARY_EXPR"):
            access, peer = "layout", None
        layout = access == "layout"
        sink = peer if access in ("read", "copy") else None
        children = list(cursor.get_children())
        if k in ("BINARY_OPERATOR", "COMPOUND_ASSIGNMENT_OPERATOR") and not layout and len(children) == 2:
            operator = self.lib.clang_getCursorBinaryOperatorKind(cursor)
            left, right = children
            if FIRST_COMPOUND <= operator <= LAST_COMPOUND:
                self.visit(left, function, "update")
                self.visit(right, function, "read", self.target(left))
                return
            if operator == ASSIGN:
                target, source = self.target(left), self.target(right)
                self.visit(left, function, "write", source)
                self.visit(right, function, "copy" if source else "read", target)
                return
        if k == "UNARY_OPERATOR" and not layout:
            operator = self.lib.clang_getCursorUnaryOperatorKind(cursor)
            if 1 <= operator <= LAST_INCREMENT:
                access, sink = "update", None
            elif operator == ADDRESS_OF:
                access, sink = "address", None
        if k == "CALL_EXPR" and not layout:
            self.call(cursor, children, function, sink)
            return
        if k in ("MEMBER_REF_EXPR", "DECL_REF_EXPR"):
            usr = self.tracked(cursor.referenced)
            if usr:
                self.record(cursor, usr, function, access, peer if access in ("read", "copy", "write") else None)
            elif k == "MEMBER_REF_EXPR" and cursor.referenced is None and cursor.spelling:
                self.loose.setdefault(cursor.spelling, self.where(cursor))
            for child in children:
                if layout:
                    self.visit(child, function, "layout")
                elif access in ("read", "copy") or child.type.kind == ci.TypeKind.POINTER:
                    self.visit(child, function, "read", sink)
                else:  # a write, update or address reaches the containing aggregate
                    self.visit(child, function, access)
            return
        if k == "MEMBER_REF":
            usr = self.tracked(cursor.referenced)
            if usr:
                self.record(cursor, usr, function, "layout" if layout else "write", None)
            return
        if k == "UNEXPOSED_EXPR" and not layout and children:
            if kind(children[0]) == "TYPE_REF" and any(kind(child) == "MEMBER_REF" for child in children):
                for child in children:  # offsetof evaluated at run time: a raw member access
                    usr = self.tracked(child.referenced) if kind(child) == "MEMBER_REF" else None
                    if usr:
                        self.record(child, usr, function, "address", None)
                return
            if kind(children[0]) == "MEMBER_REF":  # designated initializer
                target = self.tracked(children[0].referenced)
                values = children[1:]
                source = self.target(values[0]) if len(values) == 1 else None
                if target:
                    self.record(children[0], target, function, "write", source)
                for value in values:
                    self.visit(value, function, "copy" if source else "read", target)
                return
        if k == "ARRAY_SUBSCRIPT_EXPR" and len(children) == 2:
            base, subscript = children
            array = strip(base).type.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY)
            if layout:
                base_access = "layout"
            elif array and access not in ("read", "copy"):
                base_access = access
            else:
                base_access = "read"
            self.visit(base, function, base_access, sink if base_access == "read" else None)
            self.visit(subscript, function, "layout" if layout else "read")
            return
        if k in ("VAR_DECL", "FIELD_DECL"):
            if k == "VAR_DECL":
                self.tracked(cursor)
            for child in children:
                if kind(child) != "TYPE_REF":
                    self.visit(child, function, "layout" if layout else "read")
            return
        if k in ("STRUCT_DECL", "UNION_DECL", "CLASS_DECL"):
            for child in children:
                if kind(child) == "FIELD_DECL":
                    self.tracked(child)
        through = k in PASS_THROUGH
        for child in children:
            child_access = access if through or layout else "read"
            self.visit(child, function, child_access, peer if through else sink)

    def call(self, cursor: ci.Cursor, children: list[ci.Cursor], function: str, sink: str | None) -> None:
        arguments = list(cursor.get_arguments())
        callee = cursor.referenced
        parameters = list(callee.get_arguments()) if callee is not None and kind(callee) in FUNCTIONS else []
        spans = {(argument.extent.start.offset, argument.extent.end.offset) for argument in arguments}
        for child in children:
            if (child.extent.start.offset, child.extent.end.offset) not in spans:
                self.visit(child, function, "read")
        method = callee is not None and kind(callee) == "CXX_METHOD"
        assignment = method and callee.spelling == "operator=" and len(arguments) == 2
        shift = 1 if assignment or (method and len(arguments) == len(parameters) + 1) else 0
        for index, argument in enumerate(arguments):
            if shift and index == 0:  # the object of a member operator call
                if assignment:
                    self.visit(argument, function, "write", self.target(arguments[1]))
                else:
                    self.visit(argument, function, "read", sink)
                continue
            access = "read"
            if strip(argument).type.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
                access = "address"
            position = index - shift
            if position < len(parameters):
                parameter = parameters[position].type
                if parameter.kind in (ci.TypeKind.LVALUEREFERENCE, ci.TypeKind.RVALUEREFERENCE) and \
                        not parameter.get_pointee().is_const_qualified():
                    access = "address"
            target = self.target(arguments[0]) if assignment else sink
            self.visit(argument, function, access, target if access == "read" else None)

    @staticmethod
    def where(cursor: ci.Cursor) -> str:
        path = relative(cursor.location.file.name) if cursor.location.file else "?"
        return f"{path}:{cursor.location.line}"

    def unit(self, path: str, arguments: list[str], index: ci.Index) -> int:
        unit = index.parse(path, args=arguments)
        errors = sum(1 for diagnostic in unit.diagnostics if diagnostic.severity >= ci.Diagnostic.Error)
        for inclusion in unit.get_includes():
            included = relative(inclusion.include.name)
            if included:
                self.included.add(included)
        for child in unit.cursor.get_children():
            location = child.location
            if location.file is None or relative(location.file.name) is None:
                continue
            key = (os.path.normcase(location.file.name), child.extent.start.offset, child.extent.end.offset)
            if key in self.seen:
                continue
            if os.path.normcase(location.file.name) != os.path.normcase(path):
                self.seen.add(key)
            self.visit(child, "")
        return errors


def work(job: tuple[str, list[list]]) -> tuple[dict, list, dict, list, list]:
    """Index (path, arguments, subject) units; `subject` names the file a failure is reported against."""
    library, units = job
    configure(Path(library))
    walker = Walker()
    index = ci.Index.create()
    failures = []
    for path, arguments, subject in units:
        errors = walker.unit(path, arguments, index)
        if errors:
            failures.append([subject, errors])
    return walker.decls, sorted(walker.refs), walker.loose, failures, sorted(walker.included)


LAYOUT_SCOPES = {"NAMESPACE", "LINKAGE_SPEC", "STRUCT_DECL", "UNION_DECL", "CLASS_DECL", "UNEXPOSED_DECL"}


def original_layout_arguments() -> list[str]:
    """A 32-bit GNU target with GCC bitfield packing, as the PS1 compiler laid records out. A non-MSVC target drops
    the MSVC system include directories, Microsoft compatibility and the /std mapping, so they are given
    explicitly."""
    sys.path.insert(0, str(ROOT / "tools" / "lint"))
    import quality  # noqa: PLC0415

    probe = OUT.parent / "layout_probe.cpp"
    probe.parent.mkdir(parents=True, exist_ok=True)
    probe.write_text("int probe;\n", encoding="utf-8")
    lines = subprocess.run([quality.llvm_tool("clang-cl"), "-v", "/E", "/TP", str(probe)], capture_output=True,
                           text=True, check=True).stderr.splitlines()
    probe.unlink()
    start = next(index for index, line in enumerate(lines) if line.startswith("#include <...>"))
    end = next(index for index, line in enumerate(lines) if line.startswith("End of search"))
    builtin = Path(lines[start + 1].strip())
    system = [line.strip() for line in lines[start + 1:end] if Path(line.strip()) != builtin]
    return ["--target=i686-pc-windows-gnu", "/clang:-mno-ms-bitfields", "/D_M_IX86=600",
            "/clang:-fms-compatibility", "/clang:-fms-compatibility-version=19.50", "/clang:-std=c++2c",
            *(f"/clang:-isystem{directory}" for directory in system)]


def layout_work(job: tuple[str, list[list], list[str]]) -> tuple[dict[str, list], list]:
    """Field USR -> [bit offset, record size] under the original layout rules, from declarations only."""
    library, units, extra = job
    configure(Path(library))
    index = ci.Index.create()
    found: dict[str, list] = {}
    failures = []
    seen: set[tuple] = set()
    for path, arguments, subject in units:
        unit = index.parse(path, args=[*arguments, *extra],
                           options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
        errors = sum(1 for diagnostic in unit.diagnostics if diagnostic.severity >= ci.Diagnostic.Error)
        if errors:
            failures.append([subject, errors])
        pending = []
        for child in unit.cursor.get_children():
            location = child.location
            if location.file is None or relative(location.file.name) is None:
                continue
            key = (os.path.normcase(location.file.name), child.extent.start.offset, child.extent.end.offset)
            if key not in seen:
                seen.add(key)
                pending.append(child)
        while pending:
            cursor = pending.pop()
            for child in cursor.get_children():
                child_kind = kind(child)
                if child_kind in LAYOUT_SCOPES:
                    pending.append(child)
                elif child_kind == "FIELD_DECL":
                    usr = child.get_usr()
                    offset, size, _owner, owner_offset, owner_size = placement(child)
                    if usr and offset is not None and size is not None:
                        found.setdefault(usr, [offset, size, owner_offset, owner_size])
    return found, failures


def commands() -> list[tuple[str, list[str], str]]:
    sys.path.insert(0, str(ROOT / "tools" / "lint"))
    import quality  # noqa: PLC0415

    files = sorted("/".join(p.relative_to(ROOT).parts) for p in NATIVE.rglob("*.cpp"))
    directory = quality.compile_commands(files)
    result = []
    for entry in json.loads((directory / "compile_commands.json").read_text(encoding="utf-8")):
        arguments = [a for a in entry["arguments"][1:] if a not in ("/c", entry["file"])]
        result.append((entry["file"], ["--driver-mode=cl", *arguments], relative(entry["file"])))
    return sorted(result)


def orphan_units(units: list[tuple[str, list[str], str]], included: set[str]) -> list[tuple[str, list[str], str]]:
    """A unit per Native header no translation unit includes, compiled with its project's arguments."""
    seen = {os.path.normcase(path) for path in included}
    headers = sorted(path for path in NATIVE.rglob("*") if path.suffix.lower() in HEADERS)
    directory_arguments, project_arguments = {}, {}
    for _path, arguments, subject in units:
        folder, project = os.path.dirname(subject), subject.split("\\")[1]
        if len(arguments) > len(directory_arguments.get(folder, ())):
            directory_arguments[folder] = arguments
        if len(arguments) > len(project_arguments.get(project, ())):
            project_arguments[project] = arguments
    directory = OUT.parent / "orphans"
    directory.mkdir(parents=True, exist_ok=True)
    result = []
    for number, header in enumerate(headers):
        subject = relative(str(header))
        project = subject.split("\\")[1]
        if os.path.normcase(subject) in seen or project not in project_arguments:
            continue
        unit = directory / f"orphan{number:04d}.cpp"
        unit.write_text(f'#include "{header}"\n', encoding="utf-8")
        arguments = directory_arguments.get(os.path.dirname(subject), project_arguments[project])
        result.append((str(unit), arguments, subject))
    return result


def run(pool: multiprocessing.pool.Pool, library: Path, units: list, jobs: int, merged: dict) -> set[str]:
    included: set[str] = set()
    chunks = [units[index::jobs] for index in range(jobs)]
    for decls, refs, loose, failures, chunk_included in pool.imap_unordered(
            work, [(str(library), chunk) for chunk in chunks if chunk]):
        for usr, entry in decls.items():
            merged["decls"].setdefault(usr, entry)
        merged["refs"].update(map(tuple, refs))
        for name, where in loose.items():
            merged["loose"][name] = min(merged["loose"].get(name, where), where)
        merged["failures"] += failures
        included.update(chunk_included)
    return included


def build(jobs: int) -> dict:
    library = libclang_path()
    units = commands()
    merged: dict = {"decls": {}, "refs": set(), "loose": {}, "failures": [], "layout_failures": []}
    with multiprocessing.Pool(jobs) as pool:
        orphans = orphan_units(units, run(pool, library, units, jobs, merged))
        run(pool, library, orphans, jobs, merged)
        everything = units + orphans
        chunks = [everything[index::jobs] for index in range(jobs)]
        extra = original_layout_arguments()
        for found, failures in pool.imap_unordered(
                layout_work, [(str(library), chunk, extra) for chunk in chunks if chunk]):
            merged["layout_failures"] += failures
            for usr, (offset, size, owner_offset, owner_size) in found.items():
                entry = merged["decls"].get(usr)
                if entry is not None and entry["kind"] == "field" and "offset32" not in entry:
                    entry["offset32"], entry["record_size32"] = offset, size
                    if "owner" in entry:
                        entry["owner_offset32"], entry["owner_size32"] = owner_offset, owner_size
    grouped: dict[str, list] = {}
    for usr, path, line, function, access, peer in sorted(merged["refs"]):
        grouped.setdefault(usr, []).append([path, line, function, access, peer])
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.strip()
    return {"revision": revision, "units": len(units), "orphans": sorted(subject for _, _, subject in orphans),
            "failures": sorted(merged["failures"]), "layout_failures": sorted(merged["layout_failures"]),
            "decls": dict(sorted(merged["decls"].items())),
            "refs": grouped, "loose": dict(sorted(merged["loose"].items()))}


def load(path: Path = OUT, rebuild: bool = False, jobs: int | None = None) -> dict:
    if rebuild or not path.is_file():
        data = build(jobs or max(1, (os.cpu_count() or 2) - 1))
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, indent=None, separators=(",", ":")), encoding="utf-8")
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=OUT)
    parser.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    args = parser.parse_args()
    data = load(args.out, rebuild=True, jobs=args.jobs)
    print(f"{data['units']} units, {len(data['decls'])} declarations, "
          f"{sum(map(len, data['refs'].values()))} references, {len(data['orphans'])} orphan headers, "
          f"{len(data['failures'])} units with errors, {len(data['layout_failures'])} with original-layout errors "
          f"-> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
