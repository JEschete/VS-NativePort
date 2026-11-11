"""Register a C++ promotion and record its evidence (#340).

  python tools\\port\\promote.py show F01818
  python tools\\port\\promote.py scaffold F01818 --case actor_creation --source <cpp> --test <cpp> --tag <tag>
  python tools\\port\\promote.py register F01818 --typed VS::Battle::Ai::CreateActorAiState
         --source Native\\Game\\src\\Battle\\Ai\\ActorCreation.cpp --live VS::Battle::Ai::CreateLiveActorAiState
         --notes "7CD68.c:934 func_800E6178 original281MIPS ..."
  python tools\\port\\promote.py prune-adaptations
  python tools\\port\\promote.py ledgers <spec.json> [--summary build\\promotion-check\\<run>\\summary.json]

register edits tools\\port\\copy_fixups.py (NATIVE_HELPER_HEADERS, OWNED_CALL_BINDINGS and
MODULE_BINDING_ENTRIES, with the module and address from FUNCTION_CATALOG, or none with --inline for
a function the original only inlined; the FIXES entry and its comment are removed), appends the
owned.csv row and adds the Live adapter to gen_solution.py's VS.Game C-header list. The header
defaults to the source's include path and the Live header to
<source stem>Live.hpp. prune-adaptations drops the recorded C++ edits (cpp_adaptations.json) that no
longer match the copier's output once the row is owned, and re-anchors the rest. Every command is
idempotent: a second run reports nothing to change.

The ledger spec is JSON. Every string may hold {placeholders}, filled from the summary's "values",
the spec's "values" and --set name=value; base (HEAD), ids, record, slice and focused_tests are
always defined.

  {"record": "VR-...", "slice": "...-slice", "focused": "[tag-a],[tag-b]",
   "functions": {"F01818": {"catalog": {"proposed_system": "Game.Battle", "basis": "unit-low"},
                            "catalog_note": "; #276 row28 ...", "promotion_note": "..."}},
   "verify": {"command": "...", "dirty": "...", "expected": "...", "limits": "...", "notes": "..."},
   "gate": {"old_bridges": "symbol:...", "ec": "EC-04", "before": "3", "after": "0",
            "evidence": "...", "notes": "..."},
   "plan": {"order": "28", "commit": "", "replace": [["old notes text", "new notes text"]]}}

FUNCTION_CATALOG rows get the catalog fields, named/understood/modernized/verified and the note
(once); CPP_PROMOTION gets an id-ordered record, VERIFY_LEDGER and GATE_SLICES a newest-first row,
MODERNIZATION_LEDGER status done and PROMOTION_PLAN the slice, commit and note replacements. Then
tools\\ledger\\gates.py, modernization.py and hazards.py run --sync and --check (skip with --no-sync).
"""

from __future__ import annotations

import argparse
import ast
import csv
import datetime
import io
import json
import re
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path, PureWindowsPath

ROOT = Path(__file__).resolve().parents[2]
PLACEHOLDER = re.compile(r"\{([a-z_][a-z0-9_]*)\}")
ADDRESS_NAME = re.compile(r"func_([0-9A-Fa-f]{8})$")
GAME_HEADERS_OPENING = '            or project == "VS.Game" and path in ('
GAME_HEADERS_CLOSING = "            ))"


def log(message: str) -> None:
    print(message)


class Repository:
    """The files a promotion touches; tests point it at a temporary copy."""

    def __init__(self, root: Path = ROOT) -> None:
        self.root = root
        self.fixups = root / "tools" / "port" / "copy_fixups.py"
        self.owned = root / "tools" / "port" / "owned.csv"
        self.adaptations = root / "tools" / "port" / "cpp_adaptations.json"
        self.solution = root / "tools" / "scaffold" / "gen_solution.py"
        self.ledgers = root / "Docs" / "ledgers"
        self.cases = root / "tools" / "port" / "differentials"
        self.tests = root / "build" / "Debug" / "VS.Tests.exe"

    def ledger(self, name: str) -> Path:
        return self.ledgers / f"{name}.csv"


# ---------------------------------------------------------------------- text and CSV editing
class TextFile:
    """A source file edited by whole lines, keeping its line endings."""

    def __init__(self, path: Path) -> None:
        self.path = path
        with path.open(encoding="utf-8", newline="") as stream:
            self.original = stream.read()
        self.eol = "\r\n" if "\r\n" in self.original else "\n"
        if self.eol == "\r\n" and self.original.count("\n") != self.original.count("\r\n"):
            raise SystemExit(f"{path}: mixed line endings; normalize the file first")
        self.lines = self.original.split(self.eol)

    def text(self) -> str:
        return self.eol.join(self.lines)

    def tree(self) -> ast.Module:
        return ast.parse(self.text().replace("\r\n", "\n"))

    def save(self) -> bool:
        text = self.text()
        if text == self.original:
            return False
        with self.path.open("w", encoding="utf-8", newline="") as stream:
            stream.write(text)
        self.original = text
        return True


def assigned(tree: ast.Module, name: str) -> ast.expr:
    for node in tree.body:
        targets = node.targets if isinstance(node, ast.Assign) else [node.target] \
            if isinstance(node, ast.AnnAssign) else []
        if any(isinstance(target, ast.Name) and target.id == name for target in targets):
            return node.value
    raise SystemExit(f"copy_fixups.py: no top-level assignment to {name}")


def literal(node: ast.expr | None) -> object:
    try:
        return ast.literal_eval(node) if node is not None else None
    except ValueError:
        return None


class CsvTable:
    """A CSV ledger whose untouched rows keep their exact original text."""

    def __init__(self, path: Path) -> None:
        self.path = path
        with path.open(encoding="utf-8", newline="") as stream:
            self.original = stream.read()
        lines = self.original.splitlines(keepends=True)
        self.eol = "\r\n" if lines and lines[0].endswith("\r\n") else "\n"
        reader = csv.reader(io.StringIO(self.original, newline=""))
        self.fields = next(reader)
        previous = reader.line_num
        self.header = "".join(lines[:previous])
        self.rows: list[list] = []  # [row dict, original text or None once changed]
        for values in reader:
            if len(values) > len(self.fields):  # legacy rows with unquoted commas in the last (free-text) field
                values = values[:len(self.fields) - 1] + [",".join(values[len(self.fields) - 1:])]
            self.rows.append([dict(zip(self.fields, values, strict=True)), "".join(lines[previous:reader.line_num])])
            previous = reader.line_num

    def find(self, key: str, value: str) -> int | None:
        matches = [index for index, (row, _) in enumerate(self.rows) if row[key] == value]
        if len(matches) > 1:
            raise SystemExit(f"{self.path.name}: {len(matches)} rows have {key}={value}")
        return matches[0] if matches else None

    def get(self, key: str, value: str) -> dict:
        index = self.find(key, value)
        if index is None:
            raise SystemExit(f"{self.path.name}: no row has {key}={value}")
        return self.rows[index][0]

    def update(self, key: str, value: str, transform: Callable[[dict], None]) -> None:
        index = self.find(key, value)
        if index is None:
            raise SystemExit(f"{self.path.name}: no row has {key}={value}")
        row = dict(self.rows[index][0])
        transform(row)
        if row != self.rows[index][0]:
            self.rows[index] = [row, None]

    def upsert(self, key: str, row: dict, position: str) -> None:
        unknown = set(row) - set(self.fields)
        if unknown:
            raise SystemExit(f"{self.path.name}: unknown columns {sorted(unknown)}")
        full = {name: row.get(name, "") for name in self.fields}
        index = self.find(key, full[key])
        if index is not None:
            if self.rows[index][0] != full:
                self.rows[index] = [full, None]
            return
        if position == "top":
            index = 0
        elif position == "sorted":
            index = next((i for i, (other, _) in enumerate(self.rows) if other[key] > full[key]), len(self.rows))
        else:
            index = len(self.rows)
        self.rows.insert(index, [full, None])

    def serialize(self, row: dict) -> str:
        output = io.StringIO(newline="")
        csv.DictWriter(output, fieldnames=self.fields, lineterminator=self.eol).writerow(row)
        return output.getvalue()

    def save(self) -> bool:
        chunks = [original if original is not None else self.serialize(row) for row, original in self.rows]
        for index in range(len(chunks) - 1):
            if not chunks[index].endswith("\n"):  # a row now followed by another needs terminating
                chunks[index] += self.eol
        text = self.header + "".join(chunks)
        if text == self.original:
            return False
        with self.path.open("w", encoding="utf-8", newline="") as stream:
            stream.write(text)
        self.original = text
        return True


# ---------------------------------------------------------------------- catalog
class Function:
    def __init__(self, row: dict, catalog: list[dict]) -> None:
        self.id, self.name, self.row = row["id"], row["name"], row
        self.file, self.line = row["file"], row["line"]
        self.module = str(PureWindowsPath(self.file.replace("/", "\\")).parent).replace("\\", "/")
        match = ADDRESS_NAME.match(self.name)
        self.address = int(match.group(1), 16) if match else None
        self.end = None
        if self.address is not None:
            later = [int(m.group(1), 16) for other in catalog
                     if other["file"].rsplit("/", 1)[0] == self.module
                     and (m := ADDRESS_NAME.match(other["name"])) and int(m.group(1), 16) > self.address]
            self.end = min(later, default=None)


def catalog_function(repository: Repository, identifier: str) -> Function:
    with repository.ledger("FUNCTION_CATALOG").open(encoding="utf-8-sig", newline="") as stream:
        catalog = list(csv.DictReader(stream))
    row = next((row for row in catalog if row["id"] == identifier), None)
    if row is None:
        raise SystemExit(f"FUNCTION_CATALOG has no row {identifier}")
    return Function(row, catalog)


def include_path(source: str) -> str:
    """Native\\Game\\src\\A\\B.cpp -> Native\\Game\\include\\A\\B.hpp."""
    parts = list(PureWindowsPath(source).parts)
    if "src" not in parts:
        raise SystemExit(f"{source}: expected a path under a src directory; pass the header explicitly")
    parts[parts.index("src")] = "include"
    return str(PureWindowsPath(*parts).with_suffix(".hpp"))


def namespace(qualified: str) -> str:
    return qualified.rsplit("::", 1)[0] + "::" if "::" in qualified else qualified


# ---------------------------------------------------------------------- register
def add_helper_header(fixups: TextFile, live: str, header: str) -> str:
    table = assigned(fixups.tree(), "NATIVE_HELPER_HEADERS")
    for key, value in zip(table.keys, table.values, strict=True):
        if literal(key) == live:
            if literal(value) != header:
                raise SystemExit(f"NATIVE_HELPER_HEADERS maps {live} to {literal(value)}, not {header}")
            return "unchanged"
    same = [value.end_lineno for key, value in zip(table.keys, table.values, strict=True)
            if isinstance(literal(key), str) and literal(key).startswith(namespace(live))]
    index = max(same) if same else table.end_lineno - 1
    fixups.lines.insert(index, f'    "{live}": r"{header}",')
    return "added"


def add_call_binding(fixups: TextFile, identifier: str, live: str) -> str:
    pattern = re.compile(r'OWNED_CALL_BINDINGS\["(\w+)"\] = "([^"]+)"$')
    matches = [(index, pattern.match(line)) for index, line in enumerate(fixups.lines)]
    matches = [(index, match) for index, match in matches if match]
    for _, match in matches:
        if match.group(1) == identifier:
            if match.group(2) != live:
                raise SystemExit(f"OWNED_CALL_BINDINGS[{identifier}] is {match.group(2)}, not {live}")
            return "unchanged"
    if not matches:
        raise SystemExit("copy_fixups.py: no OWNED_CALL_BINDINGS[...] assignment to place the binding after")
    same = [index for index, match in matches if match.group(2).startswith(namespace(live))]
    fixups.lines.insert((same or [index for index, _ in matches])[-1] + 1,
                        f'OWNED_CALL_BINDINGS["{identifier}"] = "{live}"')
    return "added"


def add_module_entry(fixups: TextFile, function: Function, live: str) -> str:
    table = assigned(fixups.tree(), "MODULE_BINDING_ENTRIES")
    for element in table.elts:
        value = literal(element)
        if value and value[0] == function.module and value[1] == function.address:
            if value[2] != live:
                raise SystemExit(f"MODULE_BINDING_ENTRIES binds {function.address:#X} to {value[2]}, not {live}")
            return "unchanged"
    same = [element.end_lineno for element in table.elts
            if isinstance(literal(element), tuple) and str(literal(element)[2]).startswith(namespace(live))]
    index = max(same) if same else table.end_lineno - 1
    fixups.lines.insert(index, f'    ("{function.module}", 0x{function.address:08X}, "{live}"),  # {function.id}')
    return "added"


def remove_fixes(fixups: TextFile, identifier: str) -> str:
    table = assigned(fixups.tree(), "FIXES")
    for key, value in zip(table.keys, table.values, strict=True):
        if literal(key) != identifier:
            continue
        start, end = key.lineno - 1, value.end_lineno - 1
        if fixups.lines[end].strip() not in ("],", "]"):
            raise SystemExit(f"FIXES[{identifier}] does not end on its own line; remove it by hand")
        comments = start
        while comments > 0 and fixups.lines[comments - 1].lstrip().startswith("#"):
            comments -= 1
        if any(identifier in line for line in fixups.lines[comments:start]):
            start = comments
        del fixups.lines[start:end + 1]
        return f"removed {end + 1 - start} lines"
    return "absent"


def upsert_owned(repository: Repository, row: dict) -> str:
    table = CsvTable(repository.owned)
    index = table.find("id", row["id"])
    if index is not None and table.rows[index][0]["since"]:
        row = row | {"since": table.rows[index][0]["since"]}  # keep the original ownership date
    table.upsert("id", row, "end")
    return "changed" if table.save() else "unchanged"


def add_c_headers(repository: Repository, source: str) -> str:
    parts = PureWindowsPath(source).parts
    if parts[:2] != ("Native", "Game"):
        raise SystemExit(f"{source}: only VS.Game Live adapters are listed automatically; pass --no-c-headers")
    entry = str(PureWindowsPath(*parts[2:]))
    solution = TextFile(repository.solution)
    start = solution.lines.index(GAME_HEADERS_OPENING)
    end = solution.lines.index(GAME_HEADERS_CLOSING, start)
    listed = [line.strip() for line in solution.lines[start + 1:end]]
    if f'r"{entry}",' in listed:
        return "unchanged"
    folder = str(PureWindowsPath(entry).parent) + "\\"
    same = [index for index in range(start + 1, end) if solution.lines[index].strip().startswith(f'r"{folder}')]
    solution.lines.insert(same[-1] + 1 if same else end, f'                r"{entry}",')
    solution.save()
    return "added"


def register(repository: Repository, args: argparse.Namespace) -> int:
    function = catalog_function(repository, args.id)
    if args.address:
        function.address = int(args.address, 16)
    header = args.header or include_path(args.source)
    fixups = TextFile(repository.fixups)
    results = {}
    if args.live:
        if args.inline and function.address is not None:
            raise SystemExit(f"{args.id} {function.name}: --inline is for functions without a module address")
        if function.address is None and not args.inline:
            raise SystemExit(f"{args.id} {function.name}: no address in the name; pass --address, or --inline "
                             "when the original only inlined it")
        live_header = args.live_header or str(PureWindowsPath(include_path(args.source)).with_name(
            PureWindowsPath(args.source).stem + "Live.hpp"))
        results["NATIVE_HELPER_HEADERS"] = add_helper_header(fixups, args.live, live_header)
        results["OWNED_CALL_BINDINGS"] = add_call_binding(fixups, args.id, args.live)
        if not args.inline:
            results["MODULE_BINDING_ENTRIES"] = add_module_entry(fixups, function, args.live)
        if not args.no_c_headers:
            live_source = str(PureWindowsPath(live_header.replace("\\include\\", "\\src\\", 1)).with_suffix(".cpp"))
            results["gen_solution C headers"] = add_c_headers(repository, live_source)
    results["FIXES"] = remove_fixes(fixups, args.id)
    fixups.save()
    results["owned.csv"] = upsert_owned(repository, {
        "id": args.id, "kind": "function", "name": args.typed, "path": args.source, "header": header,
        "since": args.since or datetime.date.today().isoformat(), "notes": args.notes})
    for name, result in results.items():
        log(f"{name}: {result}")
    own_binding = f'OWNED_CALL_BINDINGS["{args.id}"]'
    leftovers = [index + 1 for index, line in enumerate(fixups.lines)
                 if f'"{args.id}"' in line and not line.startswith(own_binding)]
    if leftovers:
        log(f"note: copy_fixups.py still names {args.id} on lines {leftovers}; check they still apply")
    return 0


# ---------------------------------------------------------------------- adaptations
def prune_hunks(table: dict, rendered: dict[str, str]) -> tuple[dict, list[str]]:
    """Keep the hunks copy_to_native.apply_cpp_adaptations would still match, re-anchored where they now are."""
    kept, dropped = {}, []
    for path, hunks in table.items():
        if path not in rendered:
            dropped += [f"{path}: whole file ({len(hunks)} hunks)"]
            continue
        lines = rendered[path].split("\n")
        cursor = shift = 0
        survivors = []
        for index, old, new in hunks:
            found = index + shift
            if found < cursor or lines[found:found + len(old)] != old:
                found = next((i for i in range(cursor, len(lines) - len(old) + 1) if lines[i:i + len(old)] == old),
                             None)
            if found is None:
                dropped.append(f"{path}:{index + 1}: {old[:1]}")
                continue
            survivors.append([found, old, new])
            cursor = found + len(old)
            shift = found - index
        if survivors:
            kept[path] = survivors
    return kept, dropped


def write_adaptations(path: Path, table: dict) -> bool:
    text = json.dumps(table, indent=0, sort_keys=True) + "\n"
    data = text.encode("utf-8", errors="surrogateescape")
    if path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


def prune_adaptations(repository: Repository, args: argparse.Namespace) -> int:
    sys.path.insert(0, str(repository.root / "tools" / "port"))
    import copy_to_native

    rendered = copy_to_native.render(copy_to_native.Copier())
    table = json.loads(repository.adaptations.read_text(encoding="utf-8", errors="surrogateescape"))
    kept, dropped = prune_hunks(table, rendered)
    for line in dropped:
        log(f"dropped {line}")
    if args.dry_run:
        log(f"{len(dropped)} hunks would be dropped")
        return 0
    changed = write_adaptations(repository.adaptations, kept)
    log(f"{len(dropped)} hunks dropped; cpp_adaptations.json {'rewritten' if changed else 'unchanged'}")
    return 0


# ---------------------------------------------------------------------- ledgers
def fill(value: object, values: dict[str, str], missing: set[str]) -> object:
    if isinstance(value, str):
        def replace(match: re.Match) -> str:
            if match.group(1) in values:
                return str(values[match.group(1)])
            missing.add(match.group(1))
            return match.group(0)

        return PLACEHOLDER.sub(replace, value)
    if isinstance(value, list):
        return [fill(item, values, missing) for item in value]
    if isinstance(value, dict):
        return {key: fill(item, values, missing) for key, item in value.items()}
    return value


def git_head(root: Path) -> str:
    return subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True,
                          check=True).stdout.strip()


def focused_tests(repository: Repository, selection: str) -> list[str]:
    if not repository.tests.is_file():
        raise SystemExit(f"{repository.tests} is missing; build Debug or give verify.references")
    listed = subprocess.run([str(repository.tests), selection, "--list-tests", "--verbosity", "quiet"],
                            capture_output=True, text=True, check=True).stdout.splitlines()
    names = sorted(name.strip() for name in listed if name.strip())
    if not names:
        raise SystemExit(f"no tests match {selection}")
    return names


def append_once(text: str, addition: str) -> str:
    return text if not addition or addition in text else text + addition


def add_item(items: str, item: str) -> str:
    present = [value for value in items.split(";") if value]
    return items if not item or item in present else ";".join([*present, item])


def apply_ledgers(repository: Repository, spec: dict, values: dict[str, str]) -> dict[str, bool]:
    functions = spec["functions"]
    ids = list(functions)
    owned = CsvTable(repository.owned)
    paths = {identifier: owned.get("id", identifier)["path"] for identifier in ids}
    record, slice_name, base = spec["record"], spec.get("slice", ""), values["base"]

    catalog = CsvTable(repository.ledger("FUNCTION_CATALOG"))
    for identifier, entry in functions.items():
        def update_catalog(row: dict, identifier: str = identifier, entry: dict = entry) -> None:
            row.update({"final_location": paths[identifier], "named": "x", "understood": "x", "modernized": "x",
                        "verified": "x"} | entry.get("catalog", {}))
            row["notes"] = append_once(row["notes"], entry.get("catalog_note", ""))

        catalog.update("id", identifier, update_catalog)

    promotion = CsvTable(repository.ledger("CPP_PROMOTION"))
    for identifier, entry in functions.items():
        promotion.upsert("id", {
            "id": identifier, "path": paths[identifier], "source_cpp": "x", "vs_namespace": "x",
            "typed_interface": "x", "no_bridge": "x", "strict_clean": "x", "behavior_evidence": "x",
            "evidence": ";".join([f"verify:{record}", *entry.get("evidence", [])]), "reviewed": base,
            "notes": entry.get("promotion_note", "")}, "sorted")

    verify = spec["verify"]
    required = [name for name in ("command", "dirty", "expected", "limits", "notes") if not verify.get(name)]
    if required:
        raise SystemExit(f"the spec's verify section needs {required}")
    references = verify.get("references") or ";".join(f"test:{name}" for name in values["test_names"])
    verification = CsvTable(repository.ledger("VERIFY_LEDGER"))
    verification.upsert("record", {
        "record": record, "ids": ";".join(ids), "kind": verify.get("kind", "unit-test"), "references": references,
        "revision": base, "configurations": verify.get("configurations", "Debug;Release"),
        "command": verify["command"], "dirty": verify["dirty"], "expected": verify["expected"],
        "outcome": verify.get("outcome", "pass"), "limits": verify["limits"], "notes": verify["notes"]}, "top")

    tables = {"FUNCTION_CATALOG": catalog, "CPP_PROMOTION": promotion, "VERIFY_LEDGER": verification}
    if "gate" in spec:
        gate = spec["gate"]
        slices = CsvTable(repository.ledger("GATE_SLICES"))
        slices.upsert("slice", {
            "slice": slice_name, "ids": ";".join(ids), "old_bridges": gate.get("old_bridges", ""),
            "ec": gate.get("ec", ""), "before": gate.get("before", ""), "after": gate.get("after", ""),
            "before_revision": base, "evidence": gate.get("evidence", f"verify:{record}"),
            "notes": gate.get("notes", "")}, "top")
        tables["GATE_SLICES"] = slices

    modernization = CsvTable(repository.ledger("MODERNIZATION_LEDGER"))
    for identifier, entry in functions.items():
        modernization.update("id", identifier,
                             lambda row, entry=entry: row.update({"status": "done"} | entry.get("modernization", {})))
    tables["MODERNIZATION_LEDGER"] = modernization

    if "plan" in spec:
        plan = spec["plan"]

        def update_plan(row: dict) -> None:
            row["gate_slice"] = add_item(row["gate_slice"], slice_name)
            row["commits"] = add_item(row["commits"], plan.get("commit", ""))
            for old, new in plan.get("replace", []):
                if old in row["notes"]:
                    row["notes"] = row["notes"].replace(old, new)
                elif new not in row["notes"]:
                    raise SystemExit(f"PROMOTION_PLAN {plan['order']}: neither the old nor the new note text is "
                                     f"present: {old[:80]!r}")

        plans = CsvTable(repository.ledger("PROMOTION_PLAN"))
        plans.update("order", str(plan["order"]), update_plan)
        tables["PROMOTION_PLAN"] = plans
    return {name: table.save() for name, table in tables.items()}


def ledgers(repository: Repository, args: argparse.Namespace) -> int:
    spec = json.loads(Path(args.spec).read_text(encoding="utf-8"))
    values: dict = {}
    if args.summary:
        values |= json.loads(Path(args.summary).read_text(encoding="utf-8")).get("values", {})
    values |= spec.get("values", {})
    values |= dict(item.split("=", 1) for item in args.set or [])
    if "base" not in values:
        values["base"] = git_head(repository.root)
    values |= {"ids": ";".join(spec["functions"]), "record": spec["record"], "slice": spec.get("slice", "")}
    verify = spec.get("verify", {})
    if not verify.get("references"):
        if not spec.get("focused"):
            raise SystemExit("the spec needs a focused test selection or verify.references")
        values["test_names"] = focused_tests(repository, spec["focused"])
        values["focused_tests"] = str(len(values["test_names"]))
    missing: set[str] = set()
    spec = fill(spec, values, missing)
    if missing:
        raise SystemExit(f"unfilled placeholders: {sorted(missing)}; pass --summary or --set name=value")
    for name, changed in apply_ledgers(repository, spec, values).items():
        log(f"{name}: {'changed' if changed else 'unchanged'}")
    if args.no_sync:
        log("next: python tools\\ledger\\gates.py --sync; modernization.py --sync; hazards.py --sync")
        return 0
    return sync_ledgers(repository)


LEDGER_TOOLS = ("gates.py", "modernization.py", "hazards.py")


def sync_ledgers(repository: Repository) -> int:
    """Sync every ledger first (each reads the others), then check them all."""
    for flags in (["--sync"], ["--check"]):
        for name in LEDGER_TOOLS:
            command = [sys.executable, str(repository.root / "tools" / "ledger" / name), *flags]
            completed = subprocess.run(command, cwd=repository.root, capture_output=True, text=True)
            log(f"{name} {flags[0]}: {'ok' if completed.returncode == 0 else 'FAILED'}")
            if completed.returncode:
                log((completed.stdout + completed.stderr)[-4000:])
                return 1
    return 0


# ---------------------------------------------------------------------- scaffold
CASE_TEMPLATE = r'''"""{id} {name} ({file}:{line}) vs {typed}."""

from __future__ import annotations

import random

from differential import Image, Machine

ID = "{id}"
ENTRY, END = 0x{entry:08X}, 0x{end:08X}  # END is the next catalogued function; check the instruction count
MODULES = ("{module}",)
DRIVER = "{case}_driver.cpp"
SOURCES = (r"{source}",)
VECTORS, SEED = 4096, {seed}
# Each mutant patches original instruction words and must change at least one vector's result.
MUTANTS: dict[str, dict[int, int]] = {{}}


def vector(rng: random.Random, number: int) -> list:
    raise NotImplementedError("one input; boundary values for the first vectors")


def run(machine: Machine, vector: list) -> tuple:
    raise NotImplementedError("stub the callees, lay out memory, set the arguments, machine.execute(ENTRY)")


def check(image: Image, vector: list, observation: tuple, native: list[int], reference: tuple) -> list[str]:
    raise NotImplementedError("compare the original observation with the driver's integers")
'''

DRIVER_TEMPLATE = r'''// Native side of tools\port\differentials\{case}.py ({id}): one vector per stdin line, one
// line of integers per vector. argv[1] is {module}.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "{include}"

extern "C" void Core_Errorf(const char*, const char*, ...) {{ std::abort(); }}

int main(int argc, char** argv) {{
  if (argc != 2) return 2;
  std::ifstream file(argv[1], std::ios::binary);
  const std::vector<unsigned char> image(std::istreambuf_iterator<char>(file), {{}});
  std::string line;
  while (std::getline(std::cin, line)) {{
    std::istringstream in(line);
    // Parse the vector, call {typed} and print what the case's check() compares.
    std::printf("\n");
  }}
}}
'''

TEST_TEMPLATE = r'''#include <catch2/catch_test_macros.hpp>

#include "{include}"

// {id} {name} ({file}:{line}).
TEST_CASE("{short} follows the original", "[{tag}]") {{
  FAIL("write the {id} tests: each original branch, then the refusals");
}}
'''


def scaffold(repository: Repository, args: argparse.Namespace) -> int:
    function = catalog_function(repository, args.id)
    if function.address is None or function.end is None:
        raise SystemExit(f"{args.id} {function.name}: no address range in the catalog")
    header = args.header or include_path(args.source)
    parts = PureWindowsPath(header).parts
    if "include" not in parts:
        raise SystemExit(f"{header}: expected a path under an include directory")
    include = "/".join(parts[parts.index("include") + 1:])
    fields = {"id": args.id, "name": function.name, "file": function.file, "line": function.line,
              "typed": args.typed or "the owned function", "short": (args.typed or args.id).rsplit("::", 1)[-1],
              "entry": function.address, "end": function.end, "module": function.module, "case": args.case,
              "source": args.source, "seed": int(args.id[1:]), "include": include, "tag": args.tag or args.case}
    outputs = {repository.cases / f"{args.case}.py": CASE_TEMPLATE,
               repository.cases / f"{args.case}_driver.cpp": DRIVER_TEMPLATE}
    if args.test:
        outputs[repository.root / args.test] = TEST_TEMPLATE
    for path, template in outputs.items():
        if path.exists():
            log(f"exists, left alone: {path.relative_to(repository.root)}")
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(template.format(**fields), encoding="utf-8")
        log(f"wrote {path.relative_to(repository.root)}")
    return 0


def show(repository: Repository, args: argparse.Namespace) -> int:
    function = catalog_function(repository, args.id)
    row = function.row
    log(f"{function.id} {function.name} {function.file}:{function.line} module {function.module}")
    if function.address is not None:
        end = f"0x{function.end:08X} ({(function.end - function.address) // 4} words)" if function.end else "unknown"
        log(f"range 0x{function.address:08X}-{end}")
    log(f"final {row['final_name']} at {row['final_location']}; cpp={row['cpp'] or '-'} verified={row['verified'] or '-'}")
    owned = CsvTable(repository.owned).find("id", args.id)
    log("owned" if owned is not None else "not owned")
    return 0


def main(argv: list[str] | None = None, repository: Repository | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("show", help="catalog facts for a function ID")
    command.add_argument("id")
    command = commands.add_parser("register", help="register the owned row and its Live binding")
    command.add_argument("id")
    command.add_argument("--typed", required=True, help="qualified typed function, e.g. VS::Battle::Ai::Name")
    command.add_argument("--source", required=True, help=r"owned source, e.g. Native\Game\src\Battle\Ai\X.cpp")
    command.add_argument("--header", help="owned header (default: the source's include path)")
    command.add_argument("--live", help="qualified Live adapter bound at the original module address")
    command.add_argument("--live-header", help="Live adapter header (default: <source stem>Live.hpp)")
    command.add_argument("--no-c-headers", action="store_true", help="do not list the Live source for C headers")
    command.add_argument("--address", help="hex module address when the catalog name has none")
    command.add_argument("--inline", action="store_true",
                         help="the original only inlined the function: bind calls but add no module entry")
    command.add_argument("--notes", required=True, help="owned.csv notes")
    command.add_argument("--since", help="owned.csv date (default today; an existing row keeps its date)")
    command = commands.add_parser("prune-adaptations", help="drop cpp_adaptations.json hunks that no longer match")
    command.add_argument("--dry-run", action="store_true")
    command = commands.add_parser("ledgers", help="apply a ledger evidence spec")
    command.add_argument("spec")
    command.add_argument("--summary", help="promotion_check.py summary.json supplying values")
    command.add_argument("--set", action="append", metavar="NAME=VALUE")
    command.add_argument("--no-sync", action="store_true", help="skip gates/modernization/hazards --sync and --check")
    command = commands.add_parser("scaffold", help="differential case, driver and test skeletons")
    command.add_argument("id")
    command.add_argument("--case", required=True, help="case name under tools\\port\\differentials")
    command.add_argument("--source", required=True, help="owned source the driver compiles")
    command.add_argument("--header", help="owned header (default: the source's include path)")
    command.add_argument("--typed", help="qualified typed function")
    command.add_argument("--test", help=r"Catch2 test file to create, e.g. Native\Tests\src\AiXTests.cpp")
    command.add_argument("--tag", help="Catch2 tag (default: the case name)")
    args = parser.parse_args(argv)
    handlers = {"show": show, "register": register, "prune-adaptations": prune_adaptations, "ledgers": ledgers,
                "scaffold": scaffold}
    return handlers[args.command](repository or Repository(), args)


if __name__ == "__main__":
    raise SystemExit(main())
