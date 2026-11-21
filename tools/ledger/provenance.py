"""Keep native `// decomp:` provenance citations pointing at the symbol they name in resources\\rood-reverse.

  python tools\\ledger\\provenance.py --check           exit 1 on a stale citation, a stale unresolved-ledger row or a
                                                        stale FUNCTION_CATALOG location
  python tools\\ledger\\provenance.py --sync            rewrite resolvable citations and catalog locations, refresh the
                                                        unresolved ledger
  python tools\\ledger\\provenance.py --sync --previous DIR
                                                        also map names the submodule dropped, through the symbol lists
                                                        and layout of DIR, an export of the previously cited decomp

A citation is `[OVERLAY ]NAME (src/PATH:LINE[-END][; was OLD]...)` inside a comment that mentions `decomp`. PATH is
relative to the decomp root, LINE is where NAME is defined there (function header, INCLUDE_ASM/INCLUDE_RODATA line,
data, type or macro definition, or assembly label) and END, when present, is a line inside that definition. When the
decomp renamed a symbol, NAME is its current name and `; was OLD` keeps the name the native code was written against.
Identity is decided by address: a `func_`/`D_` name carries it, the decomp's symbol_addrs.txt lists name it, and a
static function without either is placed by the addressed definitions around it in the overlay's splat layout. Text
search only locates the definition of a name that identity has already established.

Docs\\ledgers\\PROVENANCE_UNRESOLVED.csv lists each citation that cannot be resolved with a concrete reason; --check
fails on any other stale citation and on a ledger row that now resolves or no longer exists.
Docs\\ledgers\\FUNCTION_CATALOG.csv `file`/`line`/`name` follow the same rule; `previous_name` keeps a renamed row's
name. Tracked in issue #493.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import io
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DECOMP = ROOT / "resources" / "rood-reverse"
UNRESOLVED_FIELDS = ["file", "line", "name", "cited", "reason"]
NATIVE_SUFFIXES = (".c", ".cpp", ".h", ".hpp", ".inl")
DECOMP_SUFFIXES = (".c", ".h", ".s", ".inc")

ADDRESS_NAME = re.compile(r"^(?:func|D|jtbl|jlabel|L|B|switchD)_([0-9A-Fa-f]{8})(?:_\w*)?$")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
SYMBOL_ADDR = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;")
ASM_LABEL = re.compile(r"^\s*(glabel|dlabel|jlabel|\.globl)\s+([A-Za-z_]\w*)")
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)")
INCLUDE_ASM = re.compile(r'\bINCLUDE_(ASM|RODATA)\s*\(\s*"[^"]*"\s*,\s*([A-Za-z_]\w*)\s*\)')
SEGMENT = re.compile(r"^\s*-\s*\[\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*([\w.]+)(?:\s*,\s*([\w./]+))?")
SEGMENT_MAP = re.compile(r"^\s*-\s*\{\s*start:\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*type:\s*([\w.]+)(?:\s*,\s*name:\s*([\w./]+))?")
VRAM = re.compile(r"^\s*vram:\s*(0x[0-9A-Fa-f]+)")
SRC_PATH = re.compile(r"^\s*src_path:\s*(\S+)")
SYMBOL_LISTS = re.compile(r"symbol_addrs_path:((?:\s*-\s*\S+)+|\s*\S+)")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else", "case", "goto", "struct", "union", "enum",
            "typedef", "static", "extern", "const", "volatile", "inline", "register", "unsigned", "signed", "void",
            "int", "char", "short", "long", "float", "double", "u_int", "u_char", "u_short", "u_long"}


@dataclass(frozen=True)
class Definition:
    path: str
    line: int
    end: int
    kind: str
    name: str
    unit: str = ""     # for a definition a shared header makes in a source file: that source file
    role: str = ""     # and the macro name the header defines it under

    @property
    def owner(self) -> str:
        return self.unit or self.path


@dataclass
class Index:
    """The definitions, symbol addresses and overlay layout of one decomp tree."""

    root: Path
    files: dict[str, list[str]] = field(default_factory=dict)
    by_name: dict[str, list[Definition]] = field(default_factory=lambda: defaultdict(list))
    by_file: dict[str, list[Definition]] = field(default_factory=lambda: defaultdict(list))
    addresses: dict[str, int] = field(default_factory=dict)
    names_at: dict[int, set[str]] = field(default_factory=lambda: defaultdict(set))
    overlays: dict[str, list[str]] = field(default_factory=dict)
    bases: dict[str, str] = field(default_factory=dict)
    emitted: dict[str, list[Definition]] = field(default_factory=dict)

    def defined(self, name: str) -> list[Definition]:
        return self.by_name.get(name, [])

    def address(self, name: str) -> int | None:
        match = ADDRESS_NAME.match(name)
        if match:
            return int(match.group(1), 16)
        return self.addresses.get(name)


def blank(text: str) -> str:
    """`text` with comments and literal contents replaced by spaces, keeping every newline and column."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + " " * (min(j, n) - i - 1) + (c if j < n and text[j] == c else ""))
            i = j + 1 if j < n and text[j] == c else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def blank_preprocessor(text: str) -> tuple[str, list[tuple[str, int]]]:
    """Remove directive lines (keeping newlines) and return the macros they define with their lines."""
    lines = text.split("\n")
    macros = []
    continued = False
    for number, line in enumerate(lines):
        if continued or line.lstrip().startswith("#"):
            match = DEFINE.match(line)
            if match and not continued:
                macros.append((match.group(1), number + 1))
            continued = line.rstrip().endswith("\\")
            lines[number] = ""
    return "\n".join(lines), macros


def name_before(head: str, stop: int) -> tuple[str, int] | None:
    """The identifier ending before `stop` in `head`, skipping whitespace, and its offset."""
    j = stop
    while j > 0 and head[j - 1].isspace():
        j -= 1
    k = j
    while k > 0 and (head[k - 1].isalnum() or head[k - 1] == "_"):
        k -= 1
    if k == j or head[k].isdigit():
        return None
    return head[k:j], k


def declarator_name(head: str) -> tuple[str, int] | None:
    """The declared name of a data declaration head without its initializer: last identifier outside brackets."""
    depth = 0
    for position in range(len(head) - 1, -1, -1):
        c = head[position]
        if c in "])":
            depth += 1
        elif c in "[(":
            depth -= 1
        elif depth == 0 and (c.isalnum() or c == "_"):
            found = name_before(head, position + 1)
            if found and found[0] not in KEYWORDS:
                return found
            return None
    return None


def scan_c(path: str, text: str) -> list[Definition]:
    """Top-level definitions of a C source or header, in source order."""
    code, macros = blank_preprocessor(blank(text))
    found = [Definition(path, line, line, "macro", name) for name, line in macros]
    starts = [0]
    for position, c in enumerate(code):
        if c == "\n":
            starts.append(position + 1)

    def line_of(offset: int) -> int:
        return bisect.bisect_right(starts, offset)

    i, n = 0, len(code)
    head_start = 0
    while i < n:
        c = code[i]
        if c not in "{;":
            i += 1
            continue
        head = code[head_start:i]
        if c == "{":
            end = matching_brace(code, i)
            if is_function_head(head):
                found += classify_body(path, head, head_start, "", end + 1, line_of, line_of(end))
                i = end + 1
            else:
                tail_end = code.find(";", end)
                tail_end = n if tail_end < 0 else tail_end
                found += classify_body(path, head, head_start, code[end + 1:tail_end], end + 1, line_of,
                                       line_of(tail_end))
                i = tail_end + 1
        else:
            found += classify_statement(path, head, head_start, line_of)
            i += 1
        head_start = i
    found.sort(key=lambda item: item.line)
    return found


def matching_brace(code: str, opening: int) -> int:
    depth = 0
    for position in range(opening, len(code)):
        if code[position] == "{":
            depth += 1
        elif code[position] == "}":
            depth -= 1
            if depth == 0:
                return position
    return len(code) - 1


def is_function_head(head: str) -> bool:
    stripped = head.strip()
    if not stripped or "=" in paren_free(stripped):
        return False
    if re.match(r"^(?:typedef\s+)?(?:struct|union|enum)\b[^()]*$", stripped):
        return False
    return stripped.endswith(")") or bool(re.search(r"\)\s*(?:__attribute__\s*\(\(.*\)\))?$", stripped))


def paren_free(text: str) -> str:
    """`text` with every parenthesized group removed."""
    out, depth = [], 0
    for c in text:
        if c == "(":
            depth += 1
        elif c == ")":
            depth = max(depth - 1, 0)
        elif depth == 0:
            out.append(c)
    return "".join(out)


def first_paren(head: str) -> int:
    """The opening parenthesis of a function head's parameter list: the group that ends the head."""
    end = head.rstrip()
    attribute = re.search(r"__attribute__\s*\(\(.*\)\)\s*$", end)
    if attribute:
        end = end[:attribute.start()].rstrip()
    if not end.endswith(")"):
        return head.find("(")
    depth = 0
    for position in range(len(end) - 1, -1, -1):
        if end[position] == ")":
            depth += 1
        elif end[position] == "(":
            depth -= 1
            if depth == 0:
                return position
    return head.find("(")


def classify_body(path: str, head: str, offset: int, tail: str, tail_offset: int, line_of, end_line: int
                  ) -> list[Definition]:
    stripped = head.strip()
    if not stripped:
        return []
    if is_function_head(head):
        paren = first_paren(head)
        found = name_before(head, paren)
        if not found:
            return []
        return [Definition(path, line_of(offset + found[1]), end_line, "function", found[0])]
    flat = paren_free(head)
    if "=" in flat:
        found = declarator_name(head[:head.index("=")])
        if not found or found[0] in KEYWORDS or head.lstrip().startswith("extern"):
            return []
        return [Definition(path, line_of(offset + found[1]), end_line, "data", found[0])]
    match = re.match(r"^\s*(?:(?:static|extern|const|volatile)\s+)*(typedef\s+)?(?:(?:const|volatile)\s+)*"
                     r"(struct|union|enum)\b\s*([A-Za-z_]\w*)?", head)
    if not match or (head.lstrip().startswith("extern") and not match.group(1)):
        return []
    start = line_of(offset + len(head) - len(head.lstrip()))
    found_items = [Definition(path, start, end_line, "type", match.group(3))] if match.group(3) else []
    kind = "type" if match.group(1) else "data"
    for declarator in tail.split(","):
        found = declarator_name(declarator)
        if found:
            found_items.append(Definition(path, start, end_line, kind, found[0]))
    return found_items


def classify_statement(path: str, head: str, offset: int, line_of) -> list[Definition]:
    stripped = head.strip()
    if not stripped:
        return []
    start = line_of(offset + len(head) - len(head.lstrip()))
    match = INCLUDE_ASM.search(head)
    if match:
        kind = "function" if match.group(1) == "ASM" else "data"
        return [Definition(path, start, start, kind, match.group(2))]
    extern = bool(re.match(r"^extern\b", stripped))
    if extern:
        stripped = stripped[len("extern"):].strip()
        if stripped.startswith('"'):
            return []
    if stripped.startswith("typedef"):
        body = stripped
        pointer = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", body)
        found = pointer.group(1) if pointer else (declarator_name(body) or (None,))[0]
        return [Definition(path, start, line_of(offset + len(head)), "type", found)] if found else []
    flat = paren_free(stripped)
    if "=" not in flat and "(" in stripped:
        paren = stripped.find("(")
        pointer = re.match(r"\(\s*\*\s*([A-Za-z_]\w*)", stripped[paren:])
        found = (pointer.group(1),) if pointer else name_before(stripped, paren)
        if not found or found[0] in KEYWORDS or len(IDENTIFIER.findall(stripped[:paren])) < (1 if pointer else 2):
            return []  # a macro statement
        return [Definition(path, start, line_of(offset + len(head)), "declaration", found[0])]
    if re.match(r"^(?:struct|union|enum)\s+\w+\s*$", stripped):
        return []
    target = stripped[:stripped.index("=")] if "=" in flat else stripped
    found = declarator_name(target)
    if not found or len(IDENTIFIER.findall(target)) < 2:
        return []
    line = start + target[:found[1]].count("\n")
    return [Definition(path, line, line_of(offset + len(head)), "declaration" if extern else "data", found[0])]


def scan_asm(path: str, text: str) -> list[Definition]:
    found = []
    for number, line in enumerate(text.split("\n"), 1):
        match = ASM_LABEL.match(line)
        if match and not match.group(2).startswith("."):
            kind = "data" if match.group(1) == "dlabel" else "function"
            found.append(Definition(path, number, number, kind, match.group(2)))
    return found


def read_text(path: Path) -> str:
    return path.read_bytes().decode("utf-8", errors="replace").replace("\r\n", "\n")


def build_index(root: Path) -> Index:
    root = Path(root)
    if not (root / "src").is_dir() or not (root / "config").is_dir():
        raise SystemExit(f"decomp root {root} has no src\\ and config\\; initialize resources/rood-reverse first")
    index = Index(root)
    for directory in ("src", "include"):
        base = root / directory
        if not base.is_dir():
            continue
        for file in sorted(base.rglob("*")):
            if not file.is_file() or file.suffix not in DECOMP_SUFFIXES:
                continue
            relative = file.relative_to(root).as_posix()
            text = read_text(file)
            index.files[relative] = text.split("\n")
            definitions = scan_asm(relative, text) if file.suffix in (".s", ".inc") else scan_c(relative, text)
            for item in definitions:
                index.by_name[item.name].append(item)
                index.by_file[relative].append(item)
    for listing in sorted((root / "config").rglob("symbol_addrs.txt")):
        for line in read_text(listing).split("\n"):
            match = SYMBOL_ADDR.match(line)
            if match:
                address = int(match.group(2), 16)
                index.addresses.setdefault(match.group(1), address)
                index.names_at[address].add(match.group(1))
    for name in list(index.by_name):
        address = index.address(name)
        if address is not None:
            index.names_at[address].add(name)
    for layout in sorted((root / "config").rglob("splat.yaml")):
        index.overlays[layout.parent.relative_to(root / "config").as_posix()] = segment_files(index, layout)
    for unit in sorted({path for paths in index.overlays.values() for path in paths if path.endswith(".c")}):
        index.emitted[unit] = emitted_definitions(index, unit)
        for item in index.emitted[unit]:
            if item.unit and item.name != item.role:
                index.by_name[item.name].append(item)
                index.by_file[item.path].append(item)
                address = index.address(item.name)
                if address is not None:
                    index.names_at[address].add(item.name)
    return index


DIRECTIVE = re.compile(r"^\s*#\s*(\w+)\s*(.*)$")
MACRO_HEAD = re.compile(r"([A-Za-z_]\w*)(\(([^)]*)\))?\s*(.*)$", re.S)
MACRO_CALL = re.compile(r"^\s*([A-Za-z_]\w*)\s*\(\s*([A-Za-z_]\w*)\s*\)\s*;")


def expand_name(name: str, macros: dict, depth: int = 0) -> str:
    """`name` after object-like macro replacement and token pasting through function-like macros."""
    if depth > 32 or name not in macros:
        return name
    params, body = macros[name]
    if params is not None:
        return name
    return expand_text(body.strip(), macros, depth + 1)


def expand_text(text: str, macros: dict, depth: int) -> str:
    if IDENTIFIER.fullmatch(text):
        return expand_name(text, macros, depth)
    call = re.fullmatch(r"([A-Za-z_]\w*)\s*\((.*)\)", text, re.S)
    if not call or call.group(1) not in macros or macros[call.group(1)][0] is None:
        return text
    params, body = macros[call.group(1)]
    args = [arg.strip() for arg in call.group(2).split(",")]
    if len(args) != len(params):
        return text
    raw = dict(zip(params, args, strict=True))
    expanded = {param: expand_text(arg, macros, depth + 1) for param, arg in raw.items()}
    pasted = re.sub(r"\s*##\s*", "##", body.strip())
    pieces = []
    for token in re.split(r"(##)", pasted):
        pieces.append(token)
    out = []
    for position, token in enumerate(pieces):
        if token == "##":
            out.append(token)
            continue
        adjacent = (position > 0 and pieces[position - 1] == "##") or (
            position + 1 < len(pieces) and pieces[position + 1] == "##")
        source = raw if adjacent else expanded
        out.append(re.sub(r"[A-Za-z_]\w*", lambda match, names=source: names.get(match.group(0), match.group(0)),
                          token))
    return expand_text("".join(out).replace("##", "").strip(), macros, depth + 1)


def emitted_definitions(index: Index, unit: str) -> list[Definition]:
    """The functions and data a source file defines, in source order, including those that the shared headers it
    includes define under names made by its macros (the decomp's effect templates)."""
    macros: dict[str, tuple[list[str] | None, str]] = {}
    out: list[Definition] = []
    visit_unit(index, unit, unit, macros, out, 0)
    return out


def visit_unit(index: Index, unit: str, path: str, macros: dict, out: list[Definition], depth: int) -> None:
    if depth > 16:
        return
    lines = index.files[path]
    starts = defaultdict(list)
    for item in index.by_file.get(path, []):
        if not item.unit and item.kind in ("function", "data", "declaration"):
            starts[item.line].append(item)
    active: list[tuple[bool, bool]] = []  # (this branch active, some branch taken)
    number = 0
    while number < len(lines):
        line = lines[number]
        number += 1
        enabled = all(state for state, _ in active)
        directive = DIRECTIVE.match(line)
        if directive:
            while line.rstrip().endswith("\\") and number < len(lines):
                line = line.rstrip()[:-1] + " " + lines[number]
                number += 1
            word, rest = directive.group(1), DIRECTIVE.match(line).group(2)
            rest = re.sub(r"/\*.*?\*/|//.*$", "", rest).strip()
            if word in ("if", "ifdef", "ifndef"):
                value = enabled and condition(word, rest, macros)
                active.append((value, value))
            elif word == "elif" and active:
                taken = active[-1][1]
                parent = all(state for state, _ in active[:-1])
                value = parent and not taken and condition("if", rest, macros)
                active[-1] = (value, taken or value)
            elif word == "else" and active:
                taken = active[-1][1]
                parent = all(state for state, _ in active[:-1])
                active[-1] = (parent and not taken, True)
            elif word == "endif" and active:
                active.pop()
            elif not enabled:
                pass
            elif word == "define":
                head = MACRO_HEAD.match(rest)
                if head:
                    params = [p.strip() for p in head.group(3).split(",")] if head.group(2) is not None and \
                        rest[len(head.group(1))] == "(" else None
                    body = head.group(4) if params is not None else rest[len(head.group(1)):]
                    macros[head.group(1)] = (params, body)
            elif word == "undef":
                macros.pop(rest.split()[0] if rest else "", None)
            elif word == "include":
                target = include_target(index, path, rest)
                if target:
                    visit_unit(index, unit, target, macros, out, depth + 1)
            continue
        if not enabled:
            continue
        for item in starts.get(number, []):
            name = expand_name(item.name, macros)
            if path == unit and name == item.name:
                out.append(item)
            elif name != item.name or item.kind != "declaration":
                out.append(Definition(item.path, item.line, item.end, item.kind, name, unit, item.name))
        call = MACRO_CALL.match(line)
        if call and call.group(1) in macros and macros[call.group(1)][0] is not None \
                and "static" in macros[call.group(1)][1]:
            out.append(Definition(path, number, number, "data", expand_name(call.group(2), macros), unit,
                                  call.group(2)))


def condition(word: str, rest: str, macros: dict) -> bool:
    if word == "ifdef":
        return rest.split()[0] in macros if rest else False
    if word == "ifndef":
        return rest.split()[0] not in macros if rest else True
    expression = re.sub(r"defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)",
                        lambda match: " 1 " if (match.group(1) or match.group(2)) in macros else " 0 ", rest)
    expression = expression.replace("&&", " and ").replace("||", " or ").replace("!", " not ")
    expression = re.sub(r"[A-Za-z_]\w*", lambda match: match.group(0) if match.group(0) in ("and", "or", "not")
                        else "0", expression)
    try:
        return bool(eval(expression, {"__builtins__": {}}))  # only digits, parentheses and boolean operators
    except Exception:
        return False


def include_target(index: Index, path: str, rest: str) -> str | None:
    match = re.match(r'"([^"]+)"', rest)
    if not match:
        return None
    name = match.group(1)
    directory = path.rsplit("/", 1)[0]
    for candidate in (f"{directory}/{name}", name, f"src/{name}"):
        candidate = re.sub(r"[^/]+/\.\./", "", candidate)
        if candidate in index.files and candidate.startswith("src/"):
            return candidate
    return None


def segment_files(index: Index, layout: Path) -> list[str]:
    """The overlay's source files in link (address) order, from its splat layout."""
    text = read_text(layout)
    match = re.search(r"^\s*src_path:\s*(\S+)", text, re.M)
    base = match.group(1) if match else "src/" + layout.parent.relative_to(index.root / "config").as_posix()
    index.bases[layout.parent.relative_to(index.root / "config").as_posix()] = base.rstrip("/")
    ordered = []
    for line in text.split("\n"):
        match = SEGMENT.match(line) or SEGMENT_MAP.match(line)
        if not match:
            continue
        kind = match.group(2)
        if kind not in ("c", "asm", "hasm"):
            continue
        name = match.group(3) or f"{int(match.group(1), 0):X}"
        for suffix in (".c", ".s"):
            candidate = f"{base}/{name}{suffix}"
            if candidate in index.files and candidate not in ordered:
                ordered.append(candidate)
                break
    return ordered


# ---------------------------------------------------------------------------------------------------------------------
# Native citations

LOCATION = re.compile(
    r"(?<![\w./@\\-])(?P<path>(?:[\w.\-]+/)*[\w\-]+(?:\.[\w\-]+)*\.(?:c|h|s|inc))"
    r"(?::(?P<line>\d+)(?:-(?P<end>\d+))?)?(?![\w/])")
OVERLAY_TOKEN = re.compile(r"^(?:[A-Z]+/)?[A-Z0-9_]+\.(?:PRG|BIN|40)$")
NOT_DECOMP = ("Native/", "tools/", "build/", "Docs/", "assets/", "resources/", "Content/", "Engine/", "Game/",
              "Desktop/", "Player/")


@dataclass
class Block:
    """Consecutive `//` comment lines of a native file, joined as one text with a map back to file positions."""

    path: str
    text: str
    starts: list[int]          # logical offset where each block line starts
    lines: list[int]           # file line index (0-based) of each block line
    columns: list[int]         # file column where each block line's comment text starts

    def position(self, offset: int) -> tuple[int, int]:
        row = bisect.bisect_right(self.starts, offset) - 1
        return self.lines[row], self.columns[row] + offset - self.starts[row]


@dataclass
class Citation:
    """One decomp location token in a native comment and the name it is attributed to."""

    path: str                 # native file, repository-relative with forward slashes
    line: int                 # 1-based native line of the location token
    column: int               # 0-based column of the location token
    token: str                # the location text as written
    cited_path: str           # as written, without the line
    cited_line: int | None
    cited_end: int | None
    name: str | None          # the attributed symbol, None for an unnamed location
    name_line: int | None = None
    name_column: int | None = None
    was: str | None = None    # the `; was OLD` annotation already present
    hints: list[str] = field(default_factory=list)


def comment_blocks(path: str, text: str) -> list[Block]:
    blocks: list[Block] = []
    current: Block | None = None
    code = blank_keep_comments(text)
    for number, line in enumerate(text.split("\n")):
        column = code[number]
        if column is None:
            current = None
            continue
        content = line[column + 2:]
        standalone = not line[:column].strip()
        if current is None or not standalone:
            current = Block(path, "", [], [], [])
            blocks.append(current)
        if current.text:
            current.text += "\n"
        current.starts.append(len(current.text))
        current.lines.append(number)
        current.columns.append(column + 2)
        current.text += content.rstrip("\r")
    return [block for block in blocks if "decomp" in block.text.lower()]


def blank_keep_comments(text: str) -> list[int | None]:
    """For each line, the column where a `//` comment starts outside literals, or None."""
    found: list[int | None] = []
    for line in text.split("\n"):
        column, i, quote = None, 0, None
        while i < len(line):
            c = line[i]
            if quote:
                if c == "\\":
                    i += 1
                elif c == quote:
                    quote = None
            elif c in "\"'":
                if c == "'" and i > 0 and line[i - 1].isalnum() and not line[:i].rstrip().endswith(("u8", "L")):
                    pass  # digit separator
                else:
                    quote = c
            elif line.startswith("//", i):
                column = i
                break
            i += 1
        found.append(column)
    return found


HEAD = re.compile(r"decomp(?:\s+\w+)?:\s*")
WAS = re.compile(r"^;\s*was\s+([A-Za-z_]\w*)")


def enclosing_paren(text: str, offset: int) -> int | None:
    depth = 0
    for position in range(offset - 1, -1, -1):
        c = text[position]
        if c == ")":
            depth += 1
        elif c == "(":
            if depth == 0:
                return position
            depth -= 1
    return None


def identifier_before(text: str, offset: int) -> tuple[str, int] | None:
    """The symbol written before `offset`: an identifier, or the root of a member path such as `a.b[0].c` or
    `T::member`, with the offset of that root."""
    j = offset
    while j > 0 and text[j - 1] in " \t\n":
        j -= 1
    k = j
    while k > 0 and (text[k - 1].isalnum() or text[k - 1] in "_.[]:>-*"):
        k -= 1
    word = text[k:j]
    match = re.match(r"[A-Za-z_]\w*", word)
    if not match or (match.end() < len(word) and word[match.end()] not in ".[:-"):
        return None
    if OVERLAY_TOKEN.match(word):
        return None
    return match.group(0), k


def last_head(text: str) -> re.Match | None:
    """The last `decomp:` marker in `text`."""
    matches = list(HEAD.finditer(text))
    return matches[-1] if matches else None


def block_citations(block: Block) -> list[Citation]:
    found: list[Citation] = []
    for match in LOCATION.finditer(block.text):
        cited = match.group("path")
        if cited.startswith(NOT_DECOMP) or "." not in cited:
            continue
        line, column = block.position(match.start())
        citation = Citation(block.path, line + 1, column, match.group(0), cited,
                            int(match.group("line")) if match.group("line") else None,
                            int(match.group("end")) if match.group("end") else None, None)
        was = WAS.match(block.text[match.end():])
        if was:
            citation.was = was.group(1)
        opening = enclosing_paren(block.text, match.start())
        if opening is not None:
            inside = block.text[opening + 1:match.start()]
            citation.hints = [word for word in inside.replace(",", " ").split() if OVERLAY_TOKEN.match(word)]
            if not LOCATION.search(inside):
                subject = identifier_before(block.text, opening)
                if subject:
                    citation.name = subject[0]
                    name_offset = subject[1]
        else:
            head = last_head(block.text[:match.start()])
            if head:
                between = block.text[head.end():match.start()]
                named = re.fullmatch(r"\s*(?:(\S+)\s+)?([A-Za-z_]\w*)\s*,\s*", between)
                if named and (named.group(1) is None or OVERLAY_TOKEN.match(named.group(1))):
                    citation.name = named.group(2)
                    name_offset = head.end() + named.start(2)
                    if named.group(1):
                        citation.hints.append(named.group(1))
        if citation.name in KEYWORDS:
            citation.name = None
        if citation.name:
            citation.name_line, citation.name_column = block.position(name_offset)
            citation.name_line += 1
            head = last_head(block.text[:name_offset])
            if head:
                words = block.text[head.end():name_offset].split()
                citation.hints += [word for word in words if OVERLAY_TOKEN.match(word)]
        found.append(citation)
    return found


def native_citations(root: Path) -> list[Citation]:
    found: list[Citation] = []
    for file in sorted((root / "Native").rglob("*")):
        if not file.is_file() or file.suffix not in NATIVE_SUFFIXES:
            continue
        relative = file.relative_to(root).as_posix()
        text = file.read_bytes().decode("utf-8").replace("\r\n", "\n")
        if "decomp" not in text.lower():
            continue
        for block in comment_blocks(relative, text):
            found += block_citations(block)
    return found


# ---------------------------------------------------------------------------------------------------------------------
# Resolution

KIND_ORDER = {"function": 0, "data": 1, "type": 2, "macro": 3, "declaration": 4}


@dataclass
class Resolution:
    """Where a citation's symbol is defined in the current decomp, or why that is unknown."""

    path: str | None = None
    line: int | None = None
    end: int | None = None
    name: str | None = None
    method: str = ""
    reason: str = ""
    widened: bool = False

    @property
    def resolved(self) -> bool:
        return self.path is not None and not self.reason


def overlay_of(index: Index, path: str) -> str | None:
    """The overlay key whose source directory holds `path`, the deepest match winning."""
    best = None
    for key, base in index.bases.items():
        if path.startswith(base + "/") and (best is None or len(base) > len(index.bases[best])):
            best = key
    return best


def normalized(lines: list[str]) -> list[str]:
    return [" ".join(line.split()) for line in lines]


class Resolver:
    """Map citations to the current decomp, with an optional previous tree for names the current one dropped."""

    def __init__(self, current: Index, previous: Index | None = None) -> None:
        self.current = current
        self.previous = previous
        self.files = set(current.files) | (set(previous.files) if previous else set())
        self.by_basename: dict[str, list[str]] = defaultdict(list)
        for path in sorted(self.files):
            self.by_basename[path.rsplit("/", 1)[-1]].append(path)
        self.sequences: dict[tuple, list[Definition]] = {}
        self.words: set[str] | None = None

    def mentioned(self, name: str) -> bool:
        if self.words is None:
            self.words = {word for lines in self.current.files.values() for line in lines
                          for word in IDENTIFIER.findall(line)}
        return name in self.words

    # -- paths --------------------------------------------------------------------------------------------------------

    def full_path(self, cited: str, hints: list[str], context: list[Counter]) -> tuple[str | None, str]:
        if cited in self.files:
            return cited, ""
        base = cited.rsplit("/", 1)[-1]
        candidates = [path for path in self.by_basename.get(base, []) if path.endswith("/" + cited)]
        if not candidates:
            return None, "no decomp file has this path"
        if len(candidates) > 1 and hints:
            narrowed = [path for path in candidates if any(hint_directory(hint) in "/" + path for hint in hints)]
            candidates = narrowed or candidates
        for directories in context:
            if len(candidates) <= 1:
                break
            scored = [(directories.get(path.rsplit("/", 1)[0], 0), path) for path in candidates]
            top = max(score for score, _ in scored)
            if top:
                candidates = [path for score, path in scored if score == top]
        if len(candidates) == 1:
            return candidates[0], ""
        return None, "relative path matches several decomp files"

    # -- definitions --------------------------------------------------------------------------------------------------

    def lookup(self, index: Index, name: str, path: str, line: int | None = None) -> tuple[Definition | None, str]:
        """The definition of `name` that `path` would see: same file, else same overlay, else a shared header."""
        overlay = overlay_of(index, path) or overlay_of(self.current, path)
        if overlay is None and self.previous is not None:
            overlay = overlay_of(self.previous, path)
        base = index.bases.get(overlay) if overlay else None
        if base is None and overlay:
            base = (self.previous.bases if self.previous else {}).get(overlay) or self.current.bases.get(overlay)
        definitions = index.defined(name)
        for pool in ([item for item in definitions if item.kind != "declaration"], definitions):
            same = [item for item in pool if item.path == path and not item.unit] or [
                item for item in pool if item.owner == path] or [item for item in pool if item.path == path]
            if same:
                return choose(same, line), ""
            scoped = [item for item in pool if base and item.owner.startswith(base + "/")]
            if not scoped:
                scoped = [item for item in pool if overlay_of(index, item.owner) is None]
            if scoped:
                files = {item.path for item in scoped}
                sources = {path for path in files if path.endswith(".c")}
                if len(files) > 1 and len(sources) == 1 and pool is not definitions:
                    scoped = [item for item in scoped if item.path in sources]
                    files = sources
                if len(files) > 1:
                    if pool is definitions:
                        return None, f"{name} is only declared, in several files ({', '.join(sorted(files))})"
                    return None, f"{name} is defined in several files ({', '.join(sorted(files))})"
                return choose(scoped, line), ""
        return None, ""

    def address_of(self, name: str) -> int | None:
        address = self.current.address(name)
        if address is None and self.previous is not None:
            address = self.previous.address(name)
        return address

    def sequence(self, index: Index, overlay: str, kind: str = "function") -> list[Definition]:
        key = (id(index), overlay, kind)
        if key not in self.sequences:
            self.sequences[key] = [item for path in index.overlays.get(overlay, [])
                                   for item in index.emitted.get(path, index.by_file.get(path, []))
                                   if item.kind == kind and item.owner == path]
        return self.sequences[key]

    def key(self, item: Definition) -> tuple[str, object]:
        address = self.address_of(item.name)
        return ("address", address) if address is not None else ("name", item.name)

    def by_file_layout(self, name: str, old_path: str) -> tuple[Definition | None, str]:
        """Place a symbol without an address by the definitions of the same sort around it in the previous and
        current versions of its own file, anchored on names and addresses both versions share."""
        if self.previous is None or old_path not in self.current.files:
            return None, ""
        old_items = [item for item in self.previous.by_file.get(old_path, []) if not item.unit]
        targets = [item for item in old_items if item.name == name and item.kind != "macro"]
        if not targets:
            return None, ""
        kind = min((item.kind for item in targets), key=KIND_ORDER.__getitem__)
        old = unique_names([item for item in old_items if item.kind == kind])
        new = unique_names([item for item in self.current.by_file.get(old_path, [])
                            if not item.unit and item.kind == kind])
        names = [item.name for item in old]
        found, reason = self.align(old, new, names.index(name), old_path, kind)
        if found and self.address_of(name) is None and not self.similar(old[names.index(name)], found):
            return None, f"{old_path} places {name} at {found.name}, whose definition differs"
        return found, reason

    def tokens(self, index: Index, item: Definition) -> set[str]:
        text = "\n".join(index.files[item.path][item.line - 1:item.end])
        return set(re.findall(r"\w+", blank(text))) - {item.name}

    def most_similar(self, old: Definition, candidates: list[Definition]) -> Definition | None:
        """Within a layout gap whose size changed, the one candidate whose body clearly shares the most tokens with
        the previous definition (which must have a body, not an INCLUDE_ASM line)."""
        if old.end - old.line < 2 or not candidates:
            return None
        before = self.tokens(self.previous, old)
        old_address = self.address_of(old.name)
        scored = []
        for item in candidates:
            address = self.address_of(item.name)
            if old_address is not None and address is not None and address != old_address:
                continue
            after = self.tokens(self.current, item)
            scored.append((len(before & after) / max(len(before | after), 1), item))
        scored.sort(key=lambda pair: -pair[0])
        if scored and scored[0][0] >= 0.5 and (len(scored) == 1 or scored[0][0] - scored[1][0] >= 0.15):
            return scored[0][1]
        return None

    def similar(self, old: Definition, new: Definition) -> bool:
        """Whether two definitions without addresses are plausibly the same symbol: the same name apart from leading
        underscores, or mostly the same tokens."""
        if old.name.lstrip("_") == new.name.lstrip("_"):
            return True
        before, after = self.tokens(self.previous, old), self.tokens(self.current, new)
        return bool(before | after) and len(before & after) / len(before | after) >= 0.6

    def by_layout(self, name: str, old_path: str) -> tuple[Definition | None, str]:
        """Place a symbol without a current name by the addressed or same-named definitions around it in the
        previous and current layouts of its overlay (functions and data each in emission order)."""
        if self.previous is None:
            return None, "no previous decomp tree to place it by layout"
        overlay = overlay_of(self.previous, old_path)
        if overlay is None or overlay not in self.current.overlays:
            return None, "cited file is in no overlay layout"
        reason = f"{name} is not a function or data definition in the previous {overlay} layout"
        for kind in ("function", "data"):
            old = self.sequence(self.previous, overlay, kind)
            positions = [index for index, item in enumerate(old) if item.name == name]
            if len(positions) != 1:
                continue
            found, reason = self.align(old, self.sequence(self.current, overlay, kind), positions[0], overlay, kind)
            if found:
                return found, ""
        return None, reason

    def align(self, old: list[Definition], new: list[Definition], position: int, overlay: str, kind: str
              ) -> tuple[Definition | None, str]:
        new_keys: dict[tuple[str, object], int] = {}
        for index, item in enumerate(new):
            new_keys.setdefault(self.key(item), index)
        before = after = None
        for index in range(position - 1, -1, -1):
            if self.key(old[index]) in new_keys:
                before = index
                break
        for index in range(position + 1, len(old)):
            if self.key(old[index]) in new_keys:
                after = index
                break
        new_before = new_keys[self.key(old[before])] if before is not None else -1
        new_after = new_keys[self.key(old[after])] if after is not None else len(new)
        old_gap = old[(before if before is not None else -1) + 1:(after if after is not None else len(old))]
        new_gap = new[new_before + 1:new_after]
        if len(old_gap) != len(new_gap) or new_before >= new_after:
            found = self.most_similar(old[position], new_gap) if new_before < new_after else None
            if found:
                return found, ""
            return None, (f"{overlay} {kind} layout between its neighbours differs ({len(old_gap)} previous, "
                          f"{len(new_gap)} current definitions)")
        found = new_gap[position - (before if before is not None else -1) - 1]
        old_address, new_address = self.address_of(old[position].name), self.address_of(found.name)
        if old_address is not None and new_address is not None and old_address != new_address:
            return None, (f"{overlay} layout places {old[position].name} at {found.name}, whose address differs")
        return found, ""

    def by_role(self, name: str, old_path: str) -> tuple[Definition | None, str]:
        """Map a symbol a shared header defined under a macro of the source file (an effect template role) to what
        the same source file's expansion of that macro defines now."""
        if self.previous is None:
            return None, ""
        overlay = overlay_of(self.previous, old_path)
        base = self.previous.bases.get(overlay) if overlay else None
        old = [item for item in self.previous.defined(name) if item.role and (
            item.owner == old_path or (base and item.owner.startswith(base + "/")))]
        if len({(item.owner, item.role) for item in old}) != 1:
            return None, ""
        unit, role = old[0].owner, old[0].role
        same_role = unique_names([item for item in self.previous.emitted.get(unit, []) if item.role == role
                                  and item.name != role])
        new = unique_names([item for item in self.current.emitted.get(unit, []) if item.role == role
                            and item.name != role])
        ordinal = [item.name for item in same_role].index(name) if name in [item.name for item in same_role] else -1
        if new and len(new) == len(same_role) and ordinal >= 0:
            return new[ordinal], ""
        if new:
            return None, (f"template role {role} of {unit} has {len(same_role)} previous and {len(new)} current "
                          "instances")
        # The template renamed its macros: pair the unit's macro-named symbols of the same sort in source order.
        def sort(item: Definition) -> str:
            return "function" if item.kind == "function" else "data"
        target = old[0]
        before = unique_names([item for item in self.previous.emitted.get(unit, [])
                               if item.role and item.name != item.role and sort(item) == sort(target)])
        after = unique_names([item for item in self.current.emitted.get(unit, [])
                              if item.role and item.name != item.role and sort(item) == sort(target)])
        names = [item.name for item in before]
        if len(before) == len(after) and target.name in names:
            return after[names.index(target.name)], ""
        return None, (f"template role {role} of {unit} has no current counterpart ({len(before)} previous, "
                      f"{len(after)} current template {sort(target)} symbols)")

    def identify(self, name: str, path: str, line: int | None) -> tuple[Definition | None, str, str]:
        """The current definition of the symbol `name` cited in `path`: (definition, method, reason)."""
        found, reason = self.lookup(self.current, name, path, line)
        if found:
            return found, "name", ""
        if reason:
            return None, "", reason
        address = self.address_of(name)
        if address is not None:
            names = sorted(self.current.names_at.get(address, set()) - {name})
            for candidate in names:
                found, reason = self.lookup(self.current, candidate, path)
                if found:
                    return found, "address", ""
            if self.previous is None or overlay_of(self.previous, path) is None:
                return None, "", (f"address 0x{address:08X} has no definition or declaration in the current decomp"
                                  if not names else f"address 0x{address:08X} ({', '.join(names)}) is defined "
                                                    "outside the cited overlay")
        found, reason = self.by_role(name, path)
        if found:
            return found, "role", ""
        found, layout_reason = self.by_layout(name, path)
        reason = reason or layout_reason
        if found:
            return found, "layout", ""
        found, file_reason = self.by_file_layout(name, path)
        if found:
            return found, "layout", ""
        reason = "; ".join(item for item in (reason, file_reason) if item)
        if address is not None:
            return None, "", (f"address 0x{address:08X} has no definition or declaration in the current decomp"
                              f" ({reason})")
        if not self.mentioned(name):
            return None, "", (f"the current decomp neither defines nor mentions {name}, and no address or layout "
                              "names its counterpart (previous copy only)")
        return None, "", reason or f"{name} is in no current or previous symbol list or layout"

    def enclosing(self, index: Index, path: str, line: int, end: int | None) -> Definition | None:
        spans = [item for item in index.by_file.get(path, [])
                 if item.line <= line <= item.end and (end is None or end <= item.end) and item.kind != "macro"]
        if not spans:
            return None
        return min(spans, key=lambda item: (item.end - item.line, KIND_ORDER[item.kind]))

    # -- citations ----------------------------------------------------------------------------------------------------

    def valid(self, path: str, name: str | None, line: int | None, end: int | None) -> bool:
        if path not in self.current.files:
            return False
        if line is None:
            return name is None or self.descriptive(name)
        if name is None or self.descriptive(name):
            if name is not None and self.local_line(self.current, path, name, line) == line:
                return True
            return self.enclosing(self.current, path, line, end) is not None
        for item in self.current.by_file.get(path, []):
            if item.name == name and item.line <= line <= item.end and (end is None or line <= end <= item.end):
                if item.kind != "declaration":
                    return True
                # A declaration stands only where the cited file sees no definition of the name.
                found, _ = self.lookup(self.current, name, path, line)
                return found is not None and found.kind == "declaration" and found.path == path
        return not self.current.defined(name) and self.local_line(self.current, path, name, line) == line

    def local_line(self, index: Index, path: str, name: str, line: int) -> int | None:
        """The line declaring `name` as a local or parameter of the definition around `line`: its first use there."""
        around = self.enclosing(index, path, line, None)
        if around is None or around.kind != "function" or around.unit:
            return None
        word = re.compile(rf"\b{re.escape(name)}\b")
        for number in range(around.line, around.end + 1):
            if word.search(blank(index.files[path][number - 1])):
                return number
        return None

    def resolve_local(self, cited: str, name: str, line: int | None) -> Resolution | None:
        """A citation of a local variable or parameter: find its function by identity, then its declaration."""
        if self.previous is None or line is None or cited not in self.previous.files or self.previous.defined(name):
            return None
        if self.local_line(self.previous, cited, name, line) != line:
            return None
        around = self.enclosing(self.previous, cited, line, None)
        found, method, reason = self.identify(around.name, cited, around.line)
        if not found:
            return Resolution(name=name, reason=f"local of {around.name}: {reason}")
        if found.kind != "function" or found.unit:
            return Resolution(name=name, reason=f"local of {around.name}, which is not a function now")
        declared = self.local_line(self.current, found.path, name, found.line)
        if declared is None:
            return Resolution(name=name, reason=f"{found.name} no longer declares the local {name}")
        return Resolution(found.path, declared, None, name, "local")

    def descriptive(self, name: str) -> bool:
        """A lowercase word that neither decomp tree defines: a description of the cited code, not a symbol."""
        return bool(re.fullmatch(r"[a-z]+", name)) and not self.current.defined(name) and not (
            self.previous is not None and self.previous.defined(name))

    def spelled(self, cited: str, name: str, line: int | None) -> str | None:
        """The decomp name of the definition around a cited line when the citation spells it without its leading
        underscores (or with extra ones) and the decomp defines nothing under the written name."""
        if self.previous is None or line is None or cited not in self.previous.files:
            return None
        if self.previous.defined(name) or self.current.defined(name) or self.address_of(name) is not None:
            return None
        variants = {item.name for item in self.previous.by_file.get(cited, [])
                    if item.name != name and item.name.lstrip("_") == name.lstrip("_") and item.kind != "macro"}
        return variants.pop() if len(variants) == 1 else None

    def resolve(self, cited: str, name: str | None, line: int | None, end: int | None) -> Resolution:
        if self.valid(cited, name, line, end):
            return Resolution(cited, line, end, name, "valid")
        if name is None:
            return self.resolve_unnamed(cited, line, end)
        local = self.resolve_local(cited, name, line)
        if local is not None:
            return local
        if self.descriptive(name):
            described = self.resolve_unnamed(cited, line, end)
            described.name = name
            return described
        variant = self.spelled(cited, name, line)
        if variant:
            name = variant
        found, method, reason = self.identify(name, cited, line)
        if not found:
            return Resolution(name=name, reason=reason)
        old = None
        if self.previous is not None and cited in self.previous.files and line is not None:
            old, _ = self.lookup(self.previous, name, cited, line)
            if old and old.path != cited:
                old = None
        start, finish, widened = self.place(old, found, cited, line, end)
        return Resolution(found.path, start, finish, found.name, method, widened=widened)

    def place(self, old: Definition | None, new: Definition, cited: str, line: int | None, end: int | None
              ) -> tuple[int, int | None, bool]:
        """The current lines for a citation of `line`..`end` inside the previous definition `old`."""
        if line is None:
            return new.line, None, False
        if old is None or not old.line <= line <= old.end:
            return (new.line, new.end if end is not None else None, False)
        if line == old.line and (end is None or end == old.end):
            return new.line, new.end if end is not None else None, False
        wanted = normalized(self.previous.files[cited][line - 1:(end or line)])
        body = normalized(self.current.files[new.path][new.line - 1:new.end])
        hits = [offset for offset in range(len(body) - len(wanted) + 1) if body[offset:offset + len(wanted)] == wanted]
        if len(hits) == 1 and any(wanted):
            start = new.line + hits[0]
            return start, (start + len(wanted) - 1) if end is not None else None, False
        return new.line, new.end, True

    def resolve_unnamed(self, cited: str, line: int | None, end: int | None) -> Resolution:
        if line is None:
            if cited in self.current.files:
                return Resolution(cited, None, None, None, "file")
            return Resolution(reason="file is absent from the current decomp")
        if self.previous is None or cited not in self.previous.files:
            if cited in self.current.files:
                return Resolution(reason="line lies inside no definition of the current decomp")
            return Resolution(reason="file is absent from the current decomp")
        old = self.enclosing(self.previous, cited, line, end)
        if old is None:
            return Resolution(reason="unnamed location lies inside no definition of the previous decomp")
        found, method, reason = self.identify(old.name, cited, old.line)
        if not found:
            return Resolution(reason=f"enclosing {old.name}: {reason}")
        start, finish, widened = self.place(old, found, cited, line, end)
        return Resolution(found.path, start, finish, None, "enclosing", widened=widened)


def unique_names(items: list[Definition]) -> list[Definition]:
    """The first definition of each name, in order; a data definition wins over an earlier declaration."""
    chosen: dict[str, Definition] = {}
    for item in items:
        if item.name not in chosen or (chosen[item.name].kind == "declaration" and item.kind != "declaration"):
            chosen[item.name] = item
    order = list(dict.fromkeys(item.name for item in items))
    return [chosen[name] for name in order]


def choose(items: list[Definition], line: int | None) -> Definition:
    if line is not None:
        inside = [item for item in items if item.line <= line <= item.end]
        if inside:
            items = inside
    return min(items, key=lambda item: (KIND_ORDER[item.kind], item.line))


def hint_directory(hint: str) -> str:
    return f"/{hint}/"


# ---------------------------------------------------------------------------------------------------------------------
# Ledgers, rewriting and the command line

CATALOG = Path("Docs") / "ledgers" / "FUNCTION_CATALOG.csv"
UNRESOLVED = Path("Docs") / "ledgers" / "PROVENANCE_UNRESOLVED.csv"
CATALOG_LEDGER = CATALOG.as_posix()
TEMPLATE_NAME = re.compile(r"^VS_\w+_FUNCTION$")


@dataclass
class Finding:
    """One citation (native comment or catalog row) and what the current decomp says about it."""

    file: str
    line: str
    name: str
    cited: str
    reason: str = ""
    edits: list[tuple[int, int, int, str]] = field(default_factory=list)  # (line, column, length, text), 0-based
    row: dict | None = None
    update: dict | None = None

    @property
    def key(self) -> tuple[str, str, str]:
        return self.file, self.name, self.cited


def location(path: str, line: int | None, end: int | None) -> str:
    if line is None:
        return path
    return f"{path}:{line}" + (f"-{end}" if end is not None and end != line else "")


def read_csv_rows(path: Path) -> tuple[list[str], list[dict[str, str]], str]:
    data = path.read_bytes().decode("utf-8-sig")
    terminator = "\r\n" if "\r\n" in data.split("\n", 1)[0] + "\n" else "\n"
    reader = csv.DictReader(io.StringIO(data, newline=""))
    return list(reader.fieldnames or []), list(reader), terminator


def write_csv_rows(path: Path, fields: list[str], rows: list[dict[str, str]], terminator: str) -> bool:
    buffer = io.StringIO(newline="")
    writer = csv.DictWriter(buffer, fieldnames=fields, lineterminator=terminator)
    writer.writeheader()
    writer.writerows(rows)
    data = buffer.getvalue().encode("utf-8")
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def file_context(resolver: Resolver, citations: list[Citation]) -> dict[str, Counter]:
    """Per native file, the decomp directories its unambiguous citations use: a relative path such as `0.c` is read
    in the directory the surrounding citations name."""
    context: dict[str, Counter] = defaultdict(Counter)
    for citation in citations:
        full, _ = resolver.full_path(citation.cited_path, citation.hints, [])
        if full:
            context[citation.path][full.rsplit("/", 1)[0]] += 1
    return context


def check_citation(resolver: Resolver, citation: Citation) -> Finding:
    finding = Finding(citation.path, str(citation.line), citation.name or "", citation.token)
    if citation.cited_path not in resolver.current.files:
        finding.reason = "the cited path is not a file of the decomp root"
    elif not resolver.valid(citation.cited_path, citation.name, citation.cited_line, citation.cited_end):
        finding.reason = ("the cited line is not inside the definition of " + citation.name if citation.name
                          else "the cited line lies inside no definition")
    return finding


def sync_citation(resolver: Resolver, citation: Citation, context: Counter) -> Finding:
    finding = check_citation(resolver, citation)
    if not finding.reason:
        return finding
    full, why = resolver.full_path(citation.cited_path, citation.hints, [context])
    if full is None:
        finding.reason = why
        return finding
    found = resolver.resolve(full, citation.name, citation.cited_line, citation.cited_end)
    if not found.resolved:
        finding.reason = found.reason
        return finding
    finding.reason = ""
    token = location(found.path, found.line, found.end if citation.cited_end is not None or found.widened else None)
    renamed = bool(citation.name and found.name and found.name != citation.name)
    if renamed and citation.was is None:
        token += f"; was {citation.name}"
    finding.edits.append((citation.line - 1, citation.column, len(citation.token), token))
    if renamed:
        finding.edits.append((citation.name_line - 1, citation.name_column, len(citation.name), found.name))
    return finding


def catalog_findings(resolver: Resolver, rows: list[dict[str, str]], sync: bool) -> list[Finding]:
    found: list[Finding] = []
    for row in rows:
        if not row.get("file"):
            continue
        cited = "src/" + row["file"] if not row["file"].startswith(("src/", "include/")) else row["file"]
        line = int(row["line"]) if row["line"].isdigit() else None
        template = bool(TEMPLATE_NAME.match(row["name"]))
        # A template row keeps the template macro name modernization.py keys on; it is checked by name where the
        # decomp still defines that name, otherwise as a location inside the template's definition.
        name = None if template and not resolver.current.defined(row["name"]) else row["name"]
        finding = Finding(CATALOG_LEDGER, row["id"], row["name"], location(cited, line, None), row=row)
        if cited in resolver.current.files and resolver.valid(cited, name, line, None):
            found.append(finding)
            continue
        finding.reason = "the catalog location is not the definition of " + row["name"]
        if sync:
            result = resolver.resolve(cited, name, line, None)
            if result.resolved and result.line is not None:
                finding.reason = ""
                update = {"file": result.path.removeprefix("src/"), "line": str(result.line)}
                if name is not None and not template and result.name != row["name"]:
                    update["name"] = result.name
                    update["previous_name"] = row.get("previous_name") or row["name"]
                finding.update = update
            elif not result.resolved:
                finding.reason = result.reason
        found.append(finding)
    return found


def apply_edits(root: Path, findings: list[Finding]) -> list[str]:
    """Rewrite the native comments, keeping each file's line endings and encoding."""
    by_file: dict[str, list[tuple[int, int, int, str]]] = defaultdict(list)
    for finding in findings:
        by_file[finding.file] += finding.edits
    changed = []
    for relative, edits in sorted(by_file.items()):
        if not edits:
            continue
        path = root / relative
        raw = path.read_bytes()
        text = raw.decode("utf-8")
        crlf = "\r\n" in text
        lines = text.replace("\r\n", "\n").split("\n")
        for line, column, length, replacement in sorted(set(edits), reverse=True):
            lines[line] = lines[line][:column] + replacement + lines[line][column + length:]
        out = "\n".join(lines)
        if crlf:
            out = out.replace("\n", "\r\n")
        data = out.encode("utf-8")
        if data != raw:
            path.write_bytes(data)
            changed.append(relative)
    return changed


def ledger_rows(findings: list[Finding], existing: dict[tuple[str, str, str], dict[str, str]], keep_reasons: bool
                ) -> list[dict[str, str]]:
    rows: dict[tuple[str, str, str], dict[str, str]] = {}
    for finding in findings:
        if not finding.reason or finding.key in rows:
            continue
        reason = finding.reason
        if keep_reasons and finding.key in existing:
            reason = existing[finding.key]["reason"]
        rows[finding.key] = {"file": finding.file, "line": finding.line, "name": finding.name, "cited": finding.cited,
                             "reason": reason}
    return sorted(rows.values(), key=lambda row: (row["file"], row["name"], row["cited"]))


def run(root: Path, decomp: Path, previous: Path | None, sync: bool, out=sys.stdout) -> int:
    current = build_index(decomp)
    resolver = Resolver(current, build_index(previous) if previous else None)
    citations = native_citations(root)
    unresolved_path = root / UNRESOLVED
    existing: dict[tuple[str, str, str], dict[str, str]] = {}
    if unresolved_path.is_file():
        _, rows, _ = read_csv_rows(unresolved_path)
        existing = {(row["file"], row["name"], row["cited"]): row for row in rows}
    catalog_path = root / CATALOG
    fields, catalog, terminator = read_csv_rows(catalog_path) if catalog_path.is_file() else ([], [], "\r\n")

    if sync:
        context = file_context(resolver, citations)
        findings = [sync_citation(resolver, citation, context[citation.path]) for citation in citations]
        changed = apply_edits(root, findings)
        catalog_found = catalog_findings(resolver, catalog, True)
        updates = 0
        for finding in catalog_found:
            if finding.update:
                finding.row.update(finding.update)
                updates += 1
        if catalog:
            if "previous_name" not in fields:
                fields.insert(fields.index("name") + 1, "previous_name")
            for row in catalog:
                row.setdefault("previous_name", "")
            write_csv_rows(catalog_path, fields, catalog, terminator)
        rows = ledger_rows(findings + catalog_found, existing, previous is None)
        write_csv_rows(unresolved_path, UNRESOLVED_FIELDS, rows, "\r\n")
        rewritten = sum(bool(finding.edits) for finding in findings)
        print(f"provenance: rewrote {rewritten} citations in {len(changed)} files, updated {updates} catalog rows; "
              f"{len(rows)} unresolved rows in {UNRESOLVED.as_posix()}", file=out)
        return 0

    findings = [check_citation(resolver, citation) for citation in citations]
    findings += catalog_findings(resolver, catalog, False)
    problems = []
    seen = set()
    for finding in findings:
        seen.add(finding.key)
        listed = finding.key in existing
        if finding.reason and not listed:
            problems.append(f"{finding.file}:{finding.line}: stale citation {finding.name or '(unnamed)'} "
                            f"{finding.cited}: {finding.reason}")
        elif not finding.reason and listed:
            problems.append(f"{finding.file}:{finding.line}: {finding.cited} resolves now; remove its "
                            f"{UNRESOLVED.as_posix()} row")
    for key, row in existing.items():
        if key not in seen:
            problems.append(f"{UNRESOLVED.as_posix()}: row {row['file']} {row['name']} {row['cited']} matches no "
                            "citation")
        elif not row.get("reason", "").strip():
            problems.append(f"{UNRESOLVED.as_posix()}: row {row['file']} {row['name']} {row['cited']} has no reason")
    for problem in problems:
        print(problem, file=out)
    print(f"provenance: {len(citations)} native citations, {len(catalog)} catalog rows, {len(existing)} listed as "
          f"unresolved, {len(problems)} problems", file=out)
    if problems:
        print("run `python tools\\ledger\\provenance.py --sync --previous <old decomp export>` and review the diff",
              file=out)
    return 1 if problems else 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--sync", action="store_true")
    parser.add_argument("--decomp", type=Path, default=DEFAULT_DECOMP, help="decomp root (default: %(default)s)")
    parser.add_argument("--previous", type=Path, help="export of the decomp tree the citations were written against")
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    return run(args.root, args.decomp, args.previous, args.sync)


if __name__ == "__main__":
    sys.exit(main())
