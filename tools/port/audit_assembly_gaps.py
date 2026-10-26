"""Reconcile assembly-backed catalog entries with native and read-only references."""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

if __package__:
    from .copy_fixups import NATIVE_FUNCTION_REDIRECTS
else:
    from copy_fixups import NATIVE_FUNCTION_REDIRECTS

REPO = Path(__file__).resolve().parents[2]
ASM_KINDS = {"asm-file", "INCLUDE_ASM", "inline-asm", "inline-asm-entry", "asm-entry"}
REFERENCES = ("rood-reverseWin", "rood-reverse", "rood-reverse-noprompt", "rood-boi")
BODY_ALIASES = {
    "F01829": ("rood-boi", r"pc\src\battle_helpers.c", "sort_candidates",
               "7E05C.s func_800E6974"),
}
TOKEN = re.compile(
    r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/',
    re.DOTALL,
)
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
LABEL = re.compile(r"^\s*(?:glabel|alabel)\s+(\w+)\s*$", re.MULTILINE)
SYMBOL = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*;", re.MULTILINE)
FIELDS = [
    "id", "system", "native_name", "native_status", "source_status", "kind",
    "original_symbol", "original_overlay", "original_declaration", "original_assembly",
    "provenance_comment", "native_definition", "c_references", "purpose", "notes",
]


@dataclass(frozen=True)
class Definition:
    name: str
    line: int
    body: str
    assembly: bool = False


def mask_literals(text: str) -> str:
    return TOKEN.sub(lambda match: re.sub(r"[^\n]", " ", match[0]), text)


def closing(text: str, start: int, left: str, right: str) -> int:
    depth = 0
    for index in range(start, len(text)):
        if text[index] == left:
            depth += 1
        elif text[index] == right:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"Unclosed {left} at character {start}")


def definitions(text: str, wanted: set[str]) -> list[Definition]:
    masked = mask_literals(text)
    found = []
    for match in CALL.finditer(masked):
        if match[1] not in wanted:
            continue
        end = closing(masked, match.end() - 1, "(", ")")
        tail = end + 1
        while tail < len(masked) and masked[tail].isspace():
            tail += 1
        qualifier = re.match(r"noexcept(?:\s*\([^)]*\))?\s*", masked[tail:])
        if qualifier:
            tail += qualifier.end()
        if tail >= len(masked) or masked[tail] != "{":
            continue
        body_end = closing(masked, tail, "{", "}")
        boundary = max(masked.rfind(char, 0, match.start()) for char in ";{}")
        prefix = masked[boundary + 1:match.start()]
        found.append(Definition(match[1], text.count("\n", 0, match.start()) + 1,
                                text[tail + 1:body_end],
                                bool(re.search(r"\b(?:__asm__?|asm)\b", prefix))))
    return found


def native_status(body: str) -> str:
    masked = mask_literals(body)
    if not re.search(r"\bVS_UNPORTED\s*\(", masked):
        return "implemented"
    remaining = re.sub(r"\bVS_UNPORTED\s*\([^)]*\)\s*;", "", masked)
    remaining = re.sub(r"\breturn\s+(?:0|NULL|nullptr|false)\s*;", "", remaining)
    remaining = re.sub(r"\breturn\s*;", "", remaining)
    remaining = re.sub(r"\b[\w:]+\s+result\s*=\s*\{\s*0\s*\}\s*;\s*return\s+result\s*;",
                       "", remaining)
    return "stub" if not remaining.strip() else "partial-unported"


def plain_source(body: str) -> bool:
    return not re.search(r"\b(?:__asm__?|asm|INCLUDE_ASM|VS_UNPORTED)\b",
                         mask_literals(body))


def overlay(path: Path) -> str | None:
    parts = path.parts
    if not parts:
        return None
    if "SLUS_010.40" in parts:
        return "SLUS_010.40"
    return next((part for part in parts if part.endswith(".PRG") or part.endswith(".BIN")),
                None)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def git_output(root: Path, *args: str) -> str:
    result = subprocess.run(["git", "--no-optional-locks", "-C", str(root), *args],
                            check=True, capture_output=True, text=True)
    return result.stdout.strip()


def reference_revision(root: Path, git_root: str) -> str:
    if root.resolve() != Path(git_root).resolve():
        return "unversioned snapshot"
    return git_output(root, "rev-parse", "HEAD")


def symbol_map(root: Path) -> dict[tuple[str, str], int]:
    result = {}
    for path in sorted((root / "config").rglob("symbol_addrs.txt")):
        for match in SYMBOL.finditer(path.read_text(encoding="utf-8")):
            result[path.parent.name, match[1]] = int(match[2], 16)
    return result


def address(row: dict[str, str], symbols: dict[tuple[str, str], int]) -> int | None:
    mapped = symbols.get((row["overlay"], row["name"]))
    if mapped is not None:
        return mapped
    match = re.fullmatch(r"(?:func_|L|D_)([0-9A-Fa-f]{8})", row["name"])
    return int(match[1], 16) if match else None


def counterpart_names(row: dict[str, str], primary: dict[tuple[str, str], int],
                      other: dict[tuple[str, str], int]) -> set[str]:
    names = {row["name"]}
    location = address(row, primary)
    if location is not None:
        names.update(name for (module, name), value in other.items()
                     if module == row["overlay"] and value == location)
    return names


def canonical_id(row: dict[str, str], catalog: dict[str, dict[str, str]]) -> str:
    visited = set()
    while True:
        if row["id"] in visited:
            raise ValueError(f"Duplicate cycle at {row['id']}")
        visited.add(row["id"])
        match = re.search(r"(?:^|;)\s*dup-of:(F\d+)", row["notes"])
        if not match:
            return row["id"]
        row = catalog[match[1]]


def template_status(row: dict[str, str]) -> tuple[str, str]:
    path = REPO / row["final_location"]
    text = path.read_text(encoding="utf-8")
    if row["kind"] == "template-body":
        implementation = path.with_suffix(".inl")
        macro = row["name"]
    else:
        if row["kind"].startswith("template:"):
            macro = row["kind"].rsplit(":", 1)[1]
        else:
            binding = re.search(r"(?:^|;)native-template:(\w+)(?:;|$)", row["notes"])
            if binding is None:
                raise ValueError(f"Missing native template declaration for {row['id']}")
            macro = binding[1]
        name = row["final_name"].replace("::", "_")
        match = re.search(rf"^\s*#define\s+{re.escape(macro)}\s+{re.escape(name)}\b",
                          text, re.MULTILINE)
        if not match:
            raise ValueError(f"Missing template binding for {row['id']}")
        section = text[match.end():]
        include = re.search(r'^#include\s+"([^"]+\.inl)"', section, re.MULTILINE)
        if not include:
            raise ValueError(f"Missing template include for {row['id']}")
        implementation = REPO / Path(*path.relative_to(REPO).parts[:3]) / include[1]
    bodies = definitions(implementation.read_text(encoding="utf-8"), {macro})
    if len(bodies) != 1:
        raise ValueError(f"Expected one template body for {row['id']}")
    body = bodies[0]
    status = native_status(body.body)
    if status == "implemented":
        status = "template-source" if row["kind"] == "template-body" else "template-instance"
    return status, f"{implementation.relative_to(REPO)}:{body.line}"


def assembly_index(root: Path, names: set[str]) -> dict[tuple[str | None, str], list[str]]:
    result: dict[tuple[str | None, str], list[str]] = collections.defaultdict(list)
    locations = [root / "src", root / "asm", root / "build" / "splat-artifacts"]
    for location in locations:
        if not location.is_dir():
            continue
        for path in sorted(location.rglob("*.s")):
            text = path.read_text(encoding="utf-8")
            module = overlay(path.relative_to(root))
            for match in LABEL.finditer(text):
                if match[1] in names:
                    line = text.count("\n", 0, match.start()) + 1
                    result[module, match[1]].append(f"{path.relative_to(REPO)}:{line}")
    return result


def native_definitions(rows: list[dict[str, str]], owned: dict[str, dict[str, str]]
                       ) -> tuple[dict[str, list[Definition]], dict[str, str]]:
    names_by_file: dict[str, set[str]] = collections.defaultdict(set)
    paths = {}
    for row in rows:
        item = owned.get(row["id"])
        path = item["path"] if item else row["final_location"]
        paths[row["id"]] = path
        if row["copy"] == "yes":
            names_by_file[path].add(row["final_name"].replace("::", "_"))
    found: dict[str, list[Definition]] = {}
    for path, names in names_by_file.items():
        file = REPO / path
        if not file.is_file():
            found[path] = []
            continue
        found[path] = definitions(file.read_text(encoding="utf-8"), names)
    return found, paths


def source_index(root: Path, names: set[str]
                 ) -> tuple[dict[tuple[str | None, str], list[str]], str, int]:
    found: dict[tuple[str | None, str], list[str]] = collections.defaultdict(list)
    digest = hashlib.sha256()
    count = 0
    for directory in ("src", r"pc\src", r"pc\include"):
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in (".c", ".h", ".cpp", ".hpp"):
                continue
            if any(part in ("third_party", "third-party", "external", "vendor", "build")
                   for part in path.relative_to(base).parts):
                continue
            raw = path.read_bytes()
            relative = path.relative_to(root)
            digest.update(str(relative).encode("utf-8") + b"\0" + raw)
            count += 1
            for function in definitions(raw.decode("utf-8"), names):
                if not function.assembly and plain_source(function.body):
                    found[overlay(relative), function.name].append(
                        f"{path.relative_to(REPO)}:{function.line}")
    return found, digest.hexdigest(), count


def write_csv(path: Path, rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, FIELDS)
        writer.writeheader()
        writer.writerows(rows)


def redirect_status(row: dict[str, str]) -> tuple[str, str] | None:
    redirect = NATIVE_FUNCTION_REDIRECTS.get(row["id"])
    if redirect is None:
        return None
    target, source = redirect
    name = row["final_name"].replace("::", "_")
    header = REPO / Path(row["final_location"]).with_suffix(".h")
    body_file = REPO / source
    if not header.is_file() or not body_file.is_file():
        return "missing-definition", ""
    macro = rf"^\s*#\s*define\s+{re.escape(name)}\s+{re.escape(target)}\s*$"
    if not re.search(macro, mask_literals(header.read_text(encoding="utf-8")), re.MULTILINE):
        return "missing-definition", ""
    bodies = definitions(body_file.read_text(encoding="utf-8"), {target})
    if len(bodies) > 1:
        raise ValueError(f"Multiple native redirect definitions for {row['id']} {target}")
    if not bodies:
        return "missing-definition", ""
    status = native_status(bodies[0].body)
    return ("alias-implemented" if status == "implemented" else status,
            f"{source}:{bodies[0].line}")


def audit(output: Path, publish: bool = False) -> dict:
    if output.resolve().is_relative_to((REPO / "resources").resolve()):
        raise ValueError("Audit output must not be inside read-only resources")
    rows = read_csv(REPO / "Docs" / "ledgers" / "FUNCTION_CATALOG.csv")
    catalog = {row["id"]: row for row in rows}
    modules_by_name: dict[str, set[str]] = collections.defaultdict(set)
    for row in rows:
        modules_by_name[row["name"]].add(row["overlay"])
    owned = {row["id"]: row for row in read_csv(REPO / "tools" / "port" / "owned.csv")}
    native, paths = native_definitions(rows, owned)
    states = {}
    for row in rows:
        if row["copy"] != "yes":
            states[row["id"]] = ("excluded-not-copied", "")
            continue
        if canonical_id(row, catalog) != row["id"]:
            continue
        if row["kind"].startswith("template") or re.search(
                r"(?:^|;)native-template:", row["notes"]):
            states[row["id"]] = template_status(row)
            continue
        redirected = redirect_status(row)
        if redirected is not None:
            states[row["id"]] = redirected
            continue
        name = row["final_name"].replace("::", "_")
        bodies = [item for item in native[paths[row["id"]]] if item.name == name]
        if len(bodies) > 1:
            raise ValueError(f"Multiple native definitions for {row['id']} {name}")
        if not bodies:
            states[row["id"]] = ("missing-definition", "")
        else:
            body = bodies[0]
            states[row["id"]] = (native_status(body.body), f"{paths[row['id']]}:{body.line}")
    for row in rows:
        if row["copy"] == "yes" and row["id"] not in states:
            state, location = states[canonical_id(row, catalog)]
            states[row["id"]] = (
                "alias-implemented" if state in
                ("implemented", "template-source", "template-instance", "alias-implemented") else state,
                location,
            )
    candidates = [row for row in rows
                  if row["kind"] in ASM_KINDS or states[row["id"]][0] in
                  ("stub", "partial-unported", "missing-definition")]
    primary = symbol_map(REPO / "resources" / "rood-reverseWin")
    references = {}
    metadata = {}
    assembly_references = {}
    for name in REFERENCES:
        root = REPO / "resources" / name
        before = git_output(root, "status", "--porcelain", "--", ".")
        symbols = symbol_map(root)
        mapped = {row["id"]: counterpart_names(row, primary, symbols) for row in candidates}
        names = set().union(*mapped.values())
        names.update(item[2] for item in BODY_ALIASES.values() if item[0] == name)
        source, fingerprint, source_count = source_index(root, names)
        assembly_references[name] = assembly_index(root, names)
        references[name] = source, mapped
        after = git_output(root, "status", "--porcelain", "--", ".")
        if before != after:
            raise RuntimeError(f"Reference changed during audit: {name}")
        git_root = git_output(root, "rev-parse", "--show-toplevel")
        metadata[name] = {
            "git_root": git_root,
            "revision": reference_revision(root, git_root),
            "status": before, "source_fingerprint_sha256": fingerprint,
            "source_files_scanned": source_count,
        }
    records = []
    for row in candidates:
        state, native_location = states[row["id"]]
        c_references = []
        for source, mapped in references.values():
            for symbol in mapped[row["id"]]:
                c_references.extend(source.get((row["overlay"], symbol), []))
                if modules_by_name.get(symbol, {row["overlay"]}) == {row["overlay"]}:
                    c_references.extend(source.get((None, symbol), []))
        c_references = sorted(set(c_references))
        if row["id"] in BODY_ALIASES:
            reference, relative, function, evidence = BODY_ALIASES[row["id"]]
            path = REPO / "resources" / reference / relative
            if evidence not in path.read_text(encoding="utf-8"):
                raise ValueError(f"Renamed-body provenance changed for {row['id']}")
            matches = [item for item in references[reference][0].get((None, function), [])
                       if item.rsplit(":", 1)[0] == str(path.relative_to(REPO))]
            if len(matches) != 1:
                raise ValueError(f"Renamed-body location changed for {row['id']}")
            c_references = sorted(set(c_references + matches))
        asm = assembly_references["rood-reverseWin"].get((row["overlay"], row["name"]), [])
        if not asm:
            for name, assembly in assembly_references.items():
                for symbol in references[name][1][row["id"]]:
                    asm.extend(assembly.get((row["overlay"], symbol), []))
            asm = sorted(set(asm))
        if row["kind"] in ("inline-asm", "inline-asm-entry"):
            asm = [f"resources\\rood-reverseWin\\src\\{row['file']}:{row['line']} (inline)"]
        source_state = "c-or-cpp-body-available" if c_references else "assembly-only"
        if source_state == "assembly-only" and not asm:
            source_state = "assembly-not-located"
        original = f"src/{row['file']}:{row['line']}"
        citation = original
        if row["kind"] in ("asm-file", "asm-entry") and asm:
            citation = asm[0].split("rood-reverseWin\\")[-1].replace("\\", "/")
        records.append({
            "id": row["id"], "system": row["final_name"].rsplit("::", 1)[0],
            "native_name": row["final_name"], "native_status": state,
            "source_status": source_state, "kind": row["kind"],
            "original_symbol": row["name"], "original_overlay": row["overlay"],
            "original_declaration": original, "original_assembly": " | ".join(asm),
            "provenance_comment": f"// decomp: {row['overlay']} {row['name']} ({citation})",
            "native_definition": native_location, "c_references": " | ".join(c_references),
            "purpose": row["purpose"], "notes": row["notes"],
        })
    records.sort(key=lambda item: (item["system"], item["original_overlay"], item["id"]))
    gaps = [item for item in records if item["native_status"] in
            ("stub", "partial-unported", "missing-definition")]
    summary = {
        "catalog_rows": len(rows),
        "port_target_rows": sum(row["copy"] == "yes" for row in rows),
        "native_status_counts": dict(collections.Counter(value[0] for value in states.values())),
        "gap_source_counts": dict(collections.Counter(item["source_status"] for item in gaps)),
        "references": metadata,
    }
    labels = source_label_coverage(rows)
    summary["source_label_coverage"] = {
        "uncataloged_sdk_labels": sum(item["classification"] == "sdk-excluded" for item in labels),
        "uncataloged_game_labels": sum(item["classification"] == "unregistered-game-entry" for item in labels),
    }
    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "assembly-coverage.csv", records)
    write_csv(output / "native-gaps.csv", gaps)
    (output / "audit.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    (output / "source-label-exceptions.json").write_text(
        json.dumps(labels, indent=2) + "\n", encoding="utf-8")
    if publish:
        publish_report(summary, records, gaps)
    print(json.dumps({key: value for key, value in summary.items() if key != "references"},
                     indent=2), flush=True)
    return summary


def publish_report(summary: dict, records: list[dict[str, str]],
                   gaps: list[dict[str, str]]) -> None:
    docs = REPO / "Docs"
    write_csv(docs / "ASSEMBLY_GAPS.csv", gaps)
    write_csv(docs / "ASSEMBLY_GAP_COVERAGE.csv", records)
    counts = summary["gap_source_counts"]
    native_counts = collections.Counter(item["native_status"] for item in gaps)
    labels = summary["source_label_coverage"]
    tagged = [item for item in records if "/7E05C.s:" in item["original_declaration"]]
    tagged_counts = collections.Counter(
        "already implemented" if item["native_status"] == "implemented" else item["source_status"]
        for item in tagged)
    lines = [
        "# Assembly-backed native gap audit",
        "",
        "Audit of the current local reference snapshots; no network update, conversion,",
        "native-code change, or resource-file write is performed.",
        "",
        "## Results",
        "",
        f"- Catalog rows checked: **{summary['catalog_rows']:,}**.",
        f"- Port-target rows reconciled: **{summary['port_target_rows']:,}**.",
        f"- Open native entries: **{len(gaps)}** "
        f"({native_counts['stub']} stubs and {native_counts['partial-unported']} partial implementations).",
        f"- Matching C/C++ reference bodies located: **{counts.get('c-or-cpp-body-available', 0)}**.",
        f"- Assembly located, but no matching C body located: **{counts.get('assembly-only', 0)}**.",
        f"- Assembly-backed declarations with no local assembly located: **{counts.get('assembly-not-located', 0)}**.",
        "",
        "[ASSEMBLY_GAPS.csv](ASSEMBLY_GAPS.csv) contains **every open entry**, with its",
        "catalog ID, native status/location, source classification, original symbol,",
        "original declaration, located assembly, proposed provenance comment, C references",
        "and purpose. Filter `source_status` for the assembly-only subset.",
        "",
        "[ASSEMBLY_GAP_COVERAGE.csv](ASSEMBLY_GAP_COVERAGE.csv) also records assembly-backed",
        "entries already implemented, aliases, and excluded/non-copied entries. These are",
        "not counted as new gaps. Templates are checked against their actual `.inl` bodies",
        "and instantiation bindings rather than counted as missing C functions.",
        "",
        "## Assembly-only entries with a local source",
        "",
        "| ID | Native name | Original source |",
        "| --- | --- | --- |",
    ]
    for item in gaps:
        if item["source_status"] == "assembly-only":
            lines.append(f"| {item['id']} | `{item['native_name']}` | "
                         f"`{item['original_assembly'].split(' | ')[0]}` |")
    lines += ["", "## Assembly not locally extracted", "",
              "The following declarations are still gaps. Their `INCLUDE_ASM`",
              "directives exist, but the corresponding instruction listings were not",
              "found in any inspected reference. Do not invent a C implementation or",
              "write splat output into a resource folder; extract any needed assembly",
              "to an ignored build directory first.", "",
              "| ID | Native name | Original declaration |", "| --- | --- | --- |"]
    for item in gaps:
        if item["source_status"] == "assembly-not-located":
            lines.append(f"| {item['id']} | `{item['native_name']}` | `{item['original_declaration']}` |")
    lines += ["", "## Tagged file: BATTLE/BATTLE.PRG/7E05C.s", "",
              f"All **{len(tagged)}** catalog entries are accounted for: "
              f"**{tagged_counts['already implemented']} already implemented**, "
              f"**{tagged_counts['c-or-cpp-body-available']} open with C counterparts**, "
              f"and **{tagged_counts['assembly-only']} open with assembly only**.", "",
              "| ID | Original symbol | Native status | Source availability |",
              "| --- | --- | --- | --- |"]
    for item in tagged:
        lines.append(f"| {item['id']} | `{item['original_symbol']}` | "
                     f"{item['native_status']} | {item['source_status']} |")
    lines += ["", "## Provenance and modernization", "",
              "Keep the repository's existing per-function provenance format immediately",
              "above each future implementation, for example:", "", "```cpp",
              "// decomp: BATTLE.PRG func_800E6974 (src/BATTLE/BATTLE.PRG/7E05C.s:114).",
              "```", "",
              "The CSV gives actual located assembly entry lines instead of relying on",
              "catalog line zero. Shared/tail-entry control flow still needs assembly review.",
              "",
              "Many counterparts are in `resources/rood-boi/pc/src`, not the upstream",
              "decomp's main `src` tree. They are **behavior references**, not an approved",
              "drop-in implementation: guest pointers, raw offsets, scratch state, partial",
              "branches, and host-specific assumptions must be checked against the original",
              "assembly before creating native C++ with typed state and ownership.",
              "Test/probe mocks are excluded. A renamed static sorting helper is matched by",
              "its explicit original-symbol provenance, not mistaken for absent C.",
              "",
              "M2C is available at `C:\\Tools\\Base\\Decompilation\\m2c\\m2c.py`.",
              "No conversion was run during this inventory. For a genuine assembly-only",
              "entry: retain M2C output and diagnostics in an ignored build directory,",
              "review it against the full assembly/control-flow context, then implement",
              "owned C++ with focused behavior tests and the provenance above.",
              "",
              "Some open compatibility entries may already be bypassed by higher-level",
              "native replacements. This is an entry-point coverage audit, **not** proof",
              "that each entry is a live gameplay defect or should be translated rather",
              "than retired. It does not certify implementations without `VS_UNPORTED`",
              "as behaviorally complete. No native gameplay issue is closed by this audit.",
              "",
              "## Reference snapshot pins", "",
              "| Reference | Revision | Production source files scanned | Source SHA-256 |",
              "| --- | --- | --- | --- |"]
    for name, item in summary["references"].items():
        lines.append(f"| `{name}` | `{item['revision']}` | {item['source_files_scanned']} | "
                     f"`{item['source_fingerprint_sha256']}` |")
    lines += ["",
              "A Git revision is recorded only for a reference repository whose",
              "discovered Git top-level is that reference root itself. A snapshot nested",
              "inside another repository is labelled `unversioned snapshot`; its enclosing",
              "repository's HEAD is not a reference revision. Source SHA-256 values pin",
              "the scanned source content in either case. Restricted read-only Git status",
              "was unchanged before/after inspection. No fetch or checkout ran.",
              "",
              f"The raw source-assembly label check found **{labels['uncataloged_game_labels']} "
              "uncataloged game glabels**",
              f"and **{labels['uncataloged_sdk_labels']} SDK/library labels**, excluded using the existing ledger rule.",
              "SDK/platform replacement completeness remains a separate audit.",
              "",
              "Rerun `python tools\\port\\audit_assembly_gaps.py --publish` after an authorized",
              "reference update to identify newly available C bodies. Compare the recorded",
              "revisions/fingerprints before reconciling any earlier assumptions.",
              ""]
    (docs / "ASSEMBLY_GAPS.md").write_text("\n".join(lines), encoding="utf-8")


def source_label_coverage(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    from tools.ledger.build_ledgers import is_sdk_path

    root = REPO / "resources" / "rood-reverseWin" / "src"
    known = {(row["overlay"], row["name"]) for row in rows}
    exceptions = []
    for path in sorted(root.rglob("*.s")):
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(root)
        module = overlay(relative)
        for match in re.finditer(r"^\s*glabel\s+(\w+)\s*$", text, re.MULTILINE):
            if (module, match[1]) not in known:
                exceptions.append({
                    "name": match[1], "source": str(path.relative_to(REPO)),
                    "classification": "sdk-excluded" if is_sdk_path(relative)
                    else "unregistered-game-entry",
                })
    return exceptions


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=REPO / "build" / "quality" / "assembly-audit")
    parser.add_argument("--publish", action="store_true", help="write the requested Docs inventory")
    args = parser.parse_args()
    # Direct script execution does not otherwise put the repository on sys.path.
    import sys

    sys.path.insert(0, str(REPO))
    audit(args.output, publish=args.publish)


if __name__ == "__main__":
    main()
