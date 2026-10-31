"""Run native-port quality tools and reject findings absent from the reviewed baseline."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import os
import re
import runpy
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
BASELINE = HERE / "baseline.json"
OUTPUT = ROOT / "build" / "quality"
NATIVE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx", ".inl"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
PYTHON_FOLDERS = ("lint", "port", "scaffold", "ledger", "naming", "test")
RETIRING_NATIVE = {
    "Native/Engine/src/Audio/Driver/PlaybackReset.cpp": "EC-02",
    "Native/Engine/src/Audio/Driver/PlaybackReset.h": "EC-02",
    "Native/Engine/include/Core/Gte.h": "EC-03",
    "Native/Engine/src/Core/Gte.cpp": "EC-03",
    "Native/Engine/include/Core/Scratch.h": "EC-05",
    "Native/Engine/src/Core/Scratch.cpp": "EC-05",
    "Native/Engine/include/Render/PageSpace.hpp": "EC-01",
    "Native/Engine/src/Render/PageSpace.cpp": "EC-01",
}


@dataclass(frozen=True)
class Finding:
    tool: str
    rule: str
    path: str
    line: int
    message: str
    context: str

    @property
    def area(self) -> str:
        if self.path.startswith("Native/Tests/"):
            return "tests"
        if self.path.startswith("Native/Player/"):
            return "tooling"
        if "/Legacy/" in self.path:
            return "legacy"
        if self.path.startswith("Native/"):
            return "native"
        return "tooling"

    @property
    def key(self) -> str:
        identity = [self.tool, self.rule, self.path, self.message, self.context]
        return hashlib.sha256(json.dumps(identity).encode()).hexdigest()


def run(arguments: list[str], *, allowed: tuple[int, ...] = (0,)) -> str:
    result = subprocess.run(
        arguments,
        cwd=ROOT,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
        env={**os.environ, "NO_COLOR": "1"},
        check=False,
    )
    if result.returncode not in allowed:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {arguments[0]}\n"
            f"{result.stdout[-6000:]}{result.stderr[-6000:]}"
        )
    return (
        result.stdout
        if arguments[1:3] == ["-m", "ruff"]
        else result.stdout + result.stderr
    )


def llvm_tool(name: str) -> str:
    package = importlib.util.find_spec(name.replace("-", "_"))
    if package and package.origin:
        suffix = ".exe" if os.name == "nt" else ""
        binary = Path(package.origin).parent / "data" / "bin" / (name + suffix)
        if binary.is_file():
            return str(binary)
    installed = shutil.which(name)
    if installed:
        return installed
    program_files = Path(os.environ.get("PROGRAMFILES", "C:/Program Files"))
    matches = sorted(
        program_files.glob(
            f"Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin/{name}.exe"
        )
    )
    if matches:
        return str(matches[-1])
    raise RuntimeError(
        f"{name} is missing; install the Visual Studio LLVM tools or add LLVM to PATH"
    )


def relative(path: str | Path) -> str:
    value = Path(path)
    return value.resolve().relative_to(ROOT).as_posix()


def discover_files() -> tuple[list[str], list[str]]:
    manifest = json.loads((ROOT / "tools/port/copy_manifest.json").read_text())
    generated = {path.replace("\\", "/") for path in manifest}
    with (ROOT / "tools/port/owned.csv").open(
        newline="", encoding="utf-8-sig"
    ) as stream:
        owned = {
            row[column].replace("\\", "/")
            for row in csv.DictReader(stream)
            for column in ("path", "header")
            if row[column]
        }
    native = sorted(
        relative(path)
        for path in (ROOT / "Native").rglob("*")
        if path.is_file()
        and path.suffix.lower() in NATIVE_SUFFIXES
        and (relative(path) not in generated or relative(path) in owned)
        and "data" not in path.relative_to(ROOT / "Native").parts
    )
    python = sorted(
        {
            relative(path)
            for folder in PYTHON_FOLDERS
            for path in (ROOT / "tools" / folder).rglob("*.py")
            if not {"node_modules", "__pycache__"}.intersection(path.parts)
        }
        | {relative(path) for path in (ROOT / "tools").glob("*.py")}
        | {
            relative(path)
            for path in (ROOT / "Native" / "Player").rglob("*.py")
            if not any(
                part in {"node_modules", "__pycache__", "venv", ".venv"}
                or part.startswith(".venv-")
                for part in path.parts
            )
        }
    )
    return native, python


def changed_files() -> set[str]:
    tracked = run(["git", "diff", "--name-only", "--diff-filter=ACMR", "HEAD", "--"])
    untracked = run(["git", "ls-files", "--others", "--exclude-standard"])
    return set((tracked + "\n" + untracked).splitlines())


def finding(tool: str, rule: str, path: str, line: int, message: str) -> Finding:
    lines = (ROOT / path).read_text(encoding="utf-8-sig", errors="replace").splitlines()
    context = lines[line - 1].strip() if 0 < line <= len(lines) else ""
    return Finding(tool, rule, path, line, message, context)


def check_format(files: list[str]) -> list[Finding]:
    executable = llvm_tool("clang-format")
    findings = []
    for path in files:
        text = run([executable, "--style=file", "--output-replacements-xml", path])
        replacements = ET.fromstring(text)
        if replacements.get("incomplete_format") == "true":
            raise RuntimeError(f"clang-format could not parse {path}")
        if replacements.findall("replacement"):
            normalized = (
                (ROOT / path).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
            )
            digest = hashlib.sha256(normalized.encode()).hexdigest()
            findings.append(
                Finding(
                    "format", "google", path, 1, "Google formatting required", digest
                )
            )
    return findings


def check_cpplint(files: list[str]) -> list[Finding]:
    findings = []
    pattern = re.compile(r"^(.*?):(\d+):\s+(.*?)\s+\[([^]]+)\]\s+\[\d+\]$")
    for start in range(0, len(files), 30):
        text = run(
            [
                sys.executable,
                "-m",
                "cpplint",
                "--extensions=c,cc,cpp,cxx,h,hpp,hxx,inl",
                *files[start : start + 30],
            ],
            allowed=(0, 1),
        )
        parsed = 0
        for line in text.splitlines():
            match = pattern.match(line)
            if match:
                parsed += 1
                path, number, message, rule = match.groups()
                findings.append(
                    finding("cpplint", rule, relative(path), int(number), message)
                )
        reported = re.search(r"Total errors found:\s*(\d+)", text)
        if reported and int(reported[1]) != parsed:
            raise RuntimeError(f"cpplint errors could not be parsed:\n{text[-4000:]}")
        if text.count("Done processing ") != len(files[start : start + 30]):
            raise RuntimeError(
                f"cpplint did not process every requested file:\n{text[-4000:]}"
            )
    return findings


def check_ruff(files: list[str]) -> list[Finding]:
    if not files:
        return []
    findings = []
    for start in range(0, len(files), 30):
        text = run(
            [
                sys.executable,
                "-m",
                "ruff",
                "check",
                "--output-format=json",
                *files[start : start + 30],
            ],
            allowed=(0, 1),
        )
        for item in json.loads(text):
            findings.append(
                finding(
                    "ruff",
                    item["code"],
                    relative(item["filename"]),
                    item["location"]["row"],
                    item["message"],
                )
            )
    return findings


def check_clones(files: list[str]) -> list[Finding]:
    node = shutil.which("node")
    executable = HERE / "node_modules/jscpd/bin/jscpd"
    if not node or not executable.is_file():
        raise RuntimeError(
            "Clone checker missing; run npm ci --prefix tools/lint --ignore-scripts"
        )
    directory = OUTPUT / "clones"
    directory.mkdir(parents=True, exist_ok=True)
    config = directory / "config.json"
    report = directory / "jscpd-report.json"
    config.write_text(
        json.dumps(
            {
                "minLines": 10,
                "minTokens": 80,
                "threshold": 100,
                "reporters": ["json"],
                "output": str(directory),
                "silent": True,
                "absolute": True,
                "gitignore": False,
            }
        ),
        encoding="utf-8",
    )
    report.unlink(missing_ok=True)
    run([node, str(executable), "--config", str(config), *files])
    document = json.loads(report.read_text(encoding="utf-8"))
    if files and document["statistics"]["total"]["sources"] == 0:
        raise RuntimeError("Clone checker analyzed no files")
    findings = []
    for duplicate in document["duplicates"]:
        first, second = sorted(
            (duplicate["firstFile"], duplicate["secondFile"]),
            key=lambda item: (relative(item["name"]), item["start"]),
        )
        path = relative(first["name"])
        other = relative(second["name"])
        lines = (
            (ROOT / path).read_text(encoding="utf-8-sig", errors="replace").splitlines()
        )
        fragment = "\n".join(
            line.strip() for line in lines[first["start"] - 1 : first["end"]]
        )
        context = hashlib.sha256(fragment.encode()).hexdigest()
        message = f"Clone with {other} ({duplicate['lines']} lines)"
        findings.append(
            Finding(
                "clones", duplicate["format"], path, first["start"], message, context
            )
        )
    return findings


def compile_commands(files: list[str]) -> Path:
    native = ROOT / "Native"
    compiler = llvm_tool("clang-cl")
    scaffold = runpy.run_path(str(ROOT / "tools/scaffold/gen_solution.py"))
    properties = ET.parse(ROOT / "Directory.Build.props").getroot()
    namespace = {"msbuild": "http://schemas.microsoft.com/developer/msbuild/2003"}
    copied_directories = properties.findtext(
        ".//msbuild:VSCopiedCIncludes", namespaces=namespace
    )
    copied_options = properties.findtext(
        ".//msbuild:VSCopiedCOptions", namespaces=namespace
    )
    if not copied_directories or not copied_options:
        raise RuntimeError("Copied-C compiler properties are missing")
    copied_includes = [
        Path(value.replace("$(VSNative)", str(native) + os.sep))
        for value in copied_directories.split(";")
    ]
    commands = []
    for path in files:
        if Path(path).suffix not in SOURCE_SUFFIXES:
            continue
        source = ROOT / path
        project = next(
            (item for item in scaffold["PROJECTS"] if source.is_relative_to(item.dir)),
            None,
        )
        includes = [
            ROOT / "vcpkg_installed/x64-windows/include",
            ROOT / "vcpkg_installed/x64-windows/include/webm",
        ]
        if project:
            includes += [project.dir / "include", project.dir / "src"]
            includes += [
                scaffold["BY_NAME"][name].dir / "include"
                for name in project.refs + project.see
            ]
        if project and scaffold["needs_c_headers"](
            project.name, str(source.relative_to(project.dir))
        ):
            includes += copied_includes
        arguments = [
            compiler,
            "/nologo",
            "/c",
            "/DWIN32_LEAN_AND_MEAN",
            "/DNOMINMAX",
            "/D_CRT_SECURE_NO_WARNINGS",
        ]
        arguments += [f"/I{directory}" for directory in includes]
        arguments += ["/TP", "/std:c++latest", "/EHsc"]
        if project and scaffold["uses_copied_settings"](
            project, str(source.relative_to(project.dir))
        ):
            arguments += copied_options.split()
        arguments.append(str(ROOT / path))
        commands.append(
            {"directory": str(ROOT), "file": str(ROOT / path), "arguments": arguments}
        )
    directory = OUTPUT / "compile"
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "compile_commands.json").write_text(
        json.dumps(commands, indent=2), encoding="utf-8"
    )
    return directory


def tidy_sources(native: list[str], selected: set[str]) -> list[str]:
    headers_changed = any(
        Path(path).suffix in NATIVE_SUFFIXES - SOURCE_SUFFIXES for path in selected
    )
    return [
        path
        for path in native
        if Path(path).suffix in SOURCE_SUFFIXES
        and (headers_changed or path in selected)
    ]


def check_tidy(files: list[str], scope: set[str]) -> list[Finding]:
    executable = llvm_tool("clang-tidy")
    database = compile_commands(files)
    findings = []
    for path in files:
        if Path(path).suffix not in SOURCE_SUFFIXES:
            continue
        report = OUTPUT / (hashlib.sha256(path.encode()).hexdigest()[:16] + ".yaml")
        report.unlink(missing_ok=True)
        text = run(
            [executable, path, "-p", str(database), f"--export-fixes={report}"],
        )
        if not report.is_file():
            if "error:" in text:
                raise RuntimeError(f"clang-tidy failed for {path}:\n{text[-5000:]}")
            continue
        document = yaml.safe_load(report.read_text(encoding="utf-8"))
        for diagnostic in document.get("Diagnostics", []):
            message = diagnostic["DiagnosticMessage"]
            if (
                diagnostic.get("Level") == "Error"
                or diagnostic["DiagnosticName"] == "clang-diagnostic-error"
            ):
                raise RuntimeError(
                    f"clang-tidy cannot analyze {path}: {message['Message']}"
                )
            try:
                target = relative(message["FilePath"])
            except ValueError:
                continue
            if target not in scope:
                continue
            contents = (ROOT / target).read_bytes()
            line = contents[: message["FileOffset"]].count(b"\n") + 1
            findings.append(
                finding(
                    "tidy",
                    diagnostic["DiagnosticName"],
                    target,
                    line,
                    message["Message"],
                )
            )
    return findings


def new_findings(findings: list[Finding], baseline: dict[str, int]) -> list[Finding]:
    remaining = Counter(baseline)
    introduced = []
    for item in findings:
        if remaining[item.key] > 0:
            remaining[item.key] -= 1
        else:
            introduced.append(item)
    return introduced


def native_tokens(path: str, source: str) -> list[tuple[str, int, int]]:
    from clang import cindex

    unit = cindex.Index.create().parse(
        path,
        args=["-x", "c++"],
        unsaved_files=[(path, source)],
        options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES,
    )
    source_file = unit.get_file(path)
    extent = cindex.SourceRange.from_locations(
        cindex.SourceLocation.from_offset(unit, source_file, 0),
        cindex.SourceLocation.from_offset(unit, source_file, len(source.encode())),
    )
    return [
        (token.spelling, token.extent.start.line, token.extent.end.line)
        for token in unit.get_tokens(extent=extent)
    ]


def reconcile_formatting(
    findings: list[Finding], baseline: dict, reference: list[Finding]
) -> dict:
    remaining = Counter({item["key"]: item["count"] for item in baseline["findings"]})
    entries = {item["key"]: dict(item) for item in baseline["findings"]}
    for item in findings:
        if remaining[item.key]:
            remaining[item.key] -= 1
    references = {}
    for original in reference:
        identity = (original.tool, original.rule, original.path, original.message)
        references.setdefault(identity, []).append(original)
    sources = {}
    contexts = {}
    migrated_regions = {}
    for item in new_findings(
        findings, {key: row["count"] for key, row in entries.items()}
    ):
        if item.tool not in {"cpplint", "tidy"} or not item.path.startswith("Native/"):
            continue
        identity = (item.tool, item.rule, item.path, item.message)
        for original in references.get(identity, []):
            if not remaining[original.key] and original.key not in migrated_regions:
                continue
            if original.key not in contexts:
                contexts[original.key] = [
                    token[0]
                    for token in native_tokens("quality_context.cc", original.context)
                ]
            expected = contexts[original.key]
            if not expected:
                continue
            if item.path not in sources:
                sources[item.path] = native_tokens(
                    str(ROOT / item.path),
                    (ROOT / item.path).read_text(encoding="utf-8-sig"),
                )
            tokens = sources[item.path]
            region = next(
                (
                    start
                    for start in range(len(tokens) - len(expected) + 1)
                    if tokens[start][1]
                    <= item.line
                    <= tokens[start + len(expected) - 1][2]
                    and [token[0] for token in tokens[start : start + len(expected)]]
                    == expected
                ),
                None,
            )
            if region is None:
                continue
            lines = migrated_regions.get(original.key, {}).get(region)
            if lines is not None and item.line not in lines:
                lines.add(item.line)
            elif remaining[original.key]:
                remaining[original.key] -= 1
                entries[original.key]["count"] -= 1
                migrated_regions.setdefault(original.key, {})[region] = {item.line}
            else:
                continue
            if item.key not in entries:
                entries[item.key] = {
                    "key": item.key,
                    "count": 0,
                    "tool": item.tool,
                    "path": item.path,
                    "rule": item.rule,
                }
            entries[item.key]["count"] += 1
            break
    return {
        **baseline,
        "findings": [row for _, row in sorted(entries.items()) if row["count"]],
    }


def deduplicate_findings(findings: list[Finding]) -> list[Finding]:
    unique = set(item for item in findings if item.tool != "clones")
    clones = [item for item in findings if item.tool == "clones"]
    return sorted(
        [*unique, *clones],
        key=lambda item: (item.tool, item.path, item.line, item.rule, item.message),
    )


def intentional_test_fixture(item: Finding) -> bool:
    return (
        item.path.startswith("Native/Tests/")
        and item.tool == "tidy"
        and item.rule == "readability-magic-numbers"
    )


def deferred_replacement_style(item: Finding) -> bool:
    if item.path not in RETIRING_NATIVE and not (
        item.path.startswith("Native/") and "/Legacy/" in item.path
    ):
        return False
    if item.tool == "tidy":
        return item.rule.startswith("readability-") or item.rule in {
            "google-runtime-int",
            "google-readability-casting",
            "google-readability-braces-around-statements",
            "google-build-using-namespace",
        }
    return item.tool == "cpplint" and (
        item.rule.startswith("whitespace/")
        or item.rule
        in {
            "runtime/int",
            "readability/casting",
            "build/namespaces",
            "build/include_what_you_use",
            "build/include_order",
        }
    )


def registered_interfaces() -> set[tuple[str, str]]:
    with (ROOT / "tools/port/owned.csv").open(
        newline="", encoding="utf-8-sig"
    ) as stream:
        return {
            (row[column].replace("\\", "/"), row["name"])
            for row in csv.DictReader(stream)
            if row["kind"] == "function"
            for column in ("path", "header")
            if row[column]
        }


def fixed_interface_advisory(item: Finding, interfaces: set[tuple[str, str]]) -> bool:
    if item.tool != "tidy" or item.rule != "bugprone-easily-swappable-parameters":
        return False
    function = re.search(r"parameters of '([^']+)'", item.message)
    return bool(function and (item.path, function[1]) in interfaces)


def relaxed_sources() -> set[str]:
    listed = (ROOT / "tools/port/relaxed_sources.txt").read_text(encoding="utf-8").split()
    return {path.replace("\\", "/") for path in listed}


def inapplicable_cpp_header_advice(item: Finding) -> bool:
    return (
        item.tool == "cpplint"
        and item.rule == "build/include_what_you_use"
        and item.path in relaxed_sources()
        and item.message == "Add #include <string> for string"
        and re.match(r"(?:[A-Za-z_]\w*\s*(?:\.|->)\s*)+string\s*=", item.context)
        is not None
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--changed",
        action="store_true",
        help="check changed/untracked owned files; clones still scan the full scope",
    )
    parser.add_argument("--files", nargs="*", help="check specific first-party files")
    parser.add_argument(
        "--tidy",
        action="store_true",
        help="include compilation-based clang-tidy analysis",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="explicitly accept current findings after review",
    )
    parser.add_argument(
        "--reconcile-formatting",
        type=Path,
        help="rekey existing native diagnostics using an accepted pre-format report",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="list default first-party scope without running tools",
    )
    args = parser.parse_args()
    native, python = discover_files()
    scope = set(native + python)
    if args.list:
        print("\n".join(sorted(scope)))
        print(f"Scope: {len(native)} native files, {len(python)} Python files")
        return 0
    if (args.update_baseline or args.reconcile_formatting) and (
        args.changed or args.files is not None
    ):
        parser.error("baseline updates require a complete scan")
    if args.reconcile_formatting and (args.update_baseline or not args.tidy):
        parser.error("format reconciliation requires --tidy without --update-baseline")
    selected = scope
    if args.changed:
        selected = scope & changed_files()
    if args.files is not None:
        requested = {relative(path) for path in args.files}
        if requested - scope:
            parser.error(
                f"not hand-maintained first-party files: {sorted(requested - scope)}"
            )
        selected &= requested
    OUTPUT.mkdir(parents=True, exist_ok=True)
    native_selected = [path for path in native if path in selected]
    python_selected = [path for path in python if path in selected]
    checks = {
        "format": lambda: check_format(native_selected),
        "cpplint": lambda: check_cpplint(native_selected),
        "ruff": lambda: check_ruff(python_selected),
        "clones": lambda: check_clones(native + python),
    }
    if args.tidy:
        checks["tidy"] = lambda: check_tidy(tidy_sources(native, selected), scope)
    findings = []
    for name, check in checks.items():
        print(f"Checking {name}...", flush=True)
        found = check()
        print(f"  {len(found)} findings", flush=True)
        findings.extend(found)
    findings = deduplicate_findings(findings)
    fixtures = [item for item in findings if intentional_test_fixture(item)]
    findings = [item for item in findings if not intentional_test_fixture(item)]
    replacement_style = [item for item in findings if deferred_replacement_style(item)]
    findings = [item for item in findings if not deferred_replacement_style(item)]
    interfaces = registered_interfaces()
    interface_notes = [
        item for item in findings if fixed_interface_advisory(item, interfaces)
    ]
    findings = [
        item for item in findings if not fixed_interface_advisory(item, interfaces)
    ]
    language_notes = [
        item for item in findings if inapplicable_cpp_header_advice(item)
    ]
    findings = [
        item for item in findings if not inapplicable_cpp_header_advice(item)
    ]
    previous = (
        json.loads(BASELINE.read_text())
        if BASELINE.is_file()
        else {"schema": 1, "findings": []}
    )
    if previous.get("schema") != 1:
        raise RuntimeError("Unsupported quality baseline schema")
    if args.reconcile_formatting:
        reference = json.loads(args.reconcile_formatting.read_text(encoding="utf-8"))
        previous = reconcile_formatting(
            findings, previous, [Finding(**item) for item in reference["findings"]]
        )
        BASELINE.write_text(json.dumps(previous, indent=2) + "\n", encoding="utf-8")
    counts = {item["key"]: item["count"] for item in previous["findings"]}
    introduced = new_findings(findings, counts)
    areas = dict(sorted(Counter(item.area for item in findings).items()))
    (OUTPUT / "report.json").write_text(
        json.dumps(
            {
                "areas": areas,
                "intentional_test_fixtures": [asdict(item) for item in fixtures],
                "deferred_replacement_style": [
                    asdict(item) for item in replacement_style
                ],
                "fixed_interface_advisories": [
                    asdict(item) for item in interface_notes
                ],
                "language_advisories": [
                    asdict(item) for item in language_notes
                ],
                "findings": [asdict(item) for item in findings],
                "new": [asdict(item) for item in introduced],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    if args.update_baseline:
        totals = Counter(item.key for item in findings)
        examples = {item.key: item for item in findings}
        preserved = [
            item for item in previous["findings"] if item["tool"] not in checks
        ]
        entries = [
            {
                "key": key,
                "count": count,
                "tool": examples[key].tool,
                "path": examples[key].path,
                "rule": examples[key].rule,
            }
            for key, count in sorted(totals.items())
        ]
        BASELINE.write_text(
            json.dumps({"schema": 1, "findings": preserved + entries}, indent=2) + "\n",
            encoding="utf-8",
        )
        print(
            f"Baseline updated explicitly: {len(findings)} findings; reports in build/quality"
        )
        return 0
    for item in introduced[:40]:
        print(f"{item.path}:{item.line}: [{item.tool}/{item.rule}] {item.message}")
    print("Areas: " + ", ".join(f"{name}={count}" for name, count in areas.items()))
    print(f"Exact test-fixture literals reported separately: {len(fixtures)}")
    print(f"Replacement-layer style reported separately: {len(replacement_style)}")
    print(
        f"Registered C interface advisories reported separately: {len(interface_notes)}"
    )
    print(f"Inapplicable C++ header advisories reported separately: {len(language_notes)}")
    print(
        f"Quality: {len(findings)} findings, {len(introduced)} new; report: build/quality/report.json"
    )
    return int(bool(introduced))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (
        OSError,
        ValueError,
        KeyError,
        TypeError,
        RuntimeError,
        yaml.YAMLError,
        subprocess.TimeoutExpired,
    ) as error:
        print(f"Quality infrastructure error: {error}", file=sys.stderr)
        raise SystemExit(2) from error
