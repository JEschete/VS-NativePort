"""Synthetic controls for the separately reported #46 gates; no disc data or live ledgers."""

from __future__ import annotations

import csv
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gates  # noqa: E402

CATALOG_FIELDS = [
    "id", "name", "copy", "final_name", "final_location", "copied", "compiles",
    "modernized", "cpp", "verified", "notes",
]
OWNED_FIELDS = ["id", "kind", "name", "path", "header", "since", "notes"]
STRICT_PROPS = """<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemDefinitionGroup><ClCompile>
    <WarningLevel>Level4</WarningLevel>
    <TreatWarningAsError>true</TreatWarningAsError>
  </ClCompile></ItemDefinitionGroup>
</Project>
"""
RELAXED_ITEM = """<ClCompile Include="{path}">
      <TreatWarningAsError>false</TreatWarningAsError>
      <WarningLevel>Level1</WarningLevel>
      <AdditionalOptions>$(VSCopiedCOptions) %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>"""
REVISION = "b6f469008129ef589e97ca549ac3b122a36ef279"
PROMOTED_SOURCE = """#include "Sys/Thing.hpp"
namespace VS::Sys {
// decomp: func_thing
int Thing(int value) {
  return value + 1;
}
}  // namespace VS::Sys
"""


def catalog_row(identifier: str, **values: str) -> dict[str, str]:
    return {
        "id": identifier, "name": f"func_{identifier}", "copy": "yes",
        "final_name": f"Gen::Item{identifier}",
        "final_location": r"Native\Game\src\Gen\Generated.c",
        "copied": "x", "compiles": "x", "modernized": "", "cpp": "",
        "verified": "", "notes": "", **values,
    }


class Fixture:
    """A minimal repository whose real gate inputs start out consistent."""

    def __init__(self, root: Path) -> None:
        self.root = root
        self.catalog = [
            catalog_row("F00001"),
            catalog_row(
                "F00002", final_name="Sys::OwnedC",
                final_location=r"Native\Game\src\Sys\OwnedC.c",
            ),
            catalog_row(
                "F00003", final_name="Sys::Thing",
                final_location=r"Native\Game\src\Sys\Thing.cpp", cpp="wip",
            ),
            catalog_row(
                "F00004", copy="no", copied="retired", final_name="Sys::Old",
                final_location=r"Native\Game\src\Sys\OwnedC.c",
                notes="retired-by:Sys_OwnedC;replaced",
            ),
            catalog_row("F00005", copy="no", copied="", final_name="", final_location=""),
        ]
        self.owned = [
            owned_row("F00002", "Sys_OwnedC", r"Native\Game\src\Sys\OwnedC.c"),
            owned_row("F00003", "Sys_Thing", r"Native\Game\src\Sys\Thing.cpp",
                      header=r"Native\Game\src\Sys\Thing.hpp"),
        ]
        self.manifest = {r"Native\Game\src\Gen\Generated.c": "hash"}
        self.baseline: list[dict[str, object]] = []
        self.promotion = [promotion_row(
            typed_interface="", no_bridge="", vs_namespace="", behavior_evidence="", evidence="", reviewed="",
        )]
        self.verify: list[dict[str, str]] = []
        self.slices: list[dict[str, str]] = []
        self.bridges = [
            {"ec": "EC-03", "path": "Native/Engine/include/Core/Gte.h", "kind": "header"},
            {"ec": "EC-03", "path": "Native/Engine/src/Core/Gte.cpp", "kind": "source"},
        ]
        self.relaxed = {"src\\Gen\\Generated.c", "src\\Sys\\OwnedC.c"}
        self.files = {
            "Native/Game/src/Gen/Generated.c": '#include "Legacy/Psx.h"\nint a;\n',
            "Native/Game/src/Sys/OwnedC.c": '#include "Legacy/Psx.h"\nint Sys_OwnedC(void) { return 1; }\n',
            "Native/Game/src/Sys/OwnedC.h": "#pragma once\n",
            "Native/Game/src/Sys/Thing.cpp": (
                '#include "Sys/Thing.hpp"\nextern "C" int Sys_Thing(void) { return 2; }\n'
            ),
            "Native/Game/src/Sys/Thing.hpp": "#pragma once\n",
            "Native/Engine/Legacy/Psx.h": '#include "Core/Gte.h"\n',
            "Native/Engine/include/Core/Gte.h": "#pragma once\n",
            "Native/Engine/src/Core/Gte.cpp": '#include "Core/Gte.h"\n',
            "Native/Tests/src/ThingTests.cpp": (
                'TEST_CASE("Thing adds one",\n          "[thing]") {}\n'
                'TEST_CASE(\n    "Thing keeps "\n    "state") {}\n'
            ),
            "Directory.Build.props": STRICT_PROPS,
            "Docs/ledgers/SUMMARY.md": "# Ledger Summary\n",
        }

    def promote(self, **claims: str) -> None:
        """Turn F00003 into a real promotion; each control then breaks one fact."""
        self.files["Native/Game/src/Sys/Thing.cpp"] = PROMOTED_SOURCE
        self.owned[1]["name"] = "Sys_Thing"
        self.catalog[2]["modernized"] = "x"
        self.catalog[2]["cpp"] = "x"
        self.promotion = [promotion_row(**claims)]

    def relax(self, relative: str) -> None:
        self.relaxed.add(relative)

    def write(self) -> gates.Workspace:
        for relative, text in self.files.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8", newline="")
        write_csv(self.root / "Docs/ledgers/FUNCTION_CATALOG.csv", CATALOG_FIELDS, self.catalog)
        write_csv(self.root / "tools/port/owned.csv", OWNED_FIELDS, self.owned)
        write_csv(self.root / "Docs/ledgers/CPP_PROMOTION.csv", gates.PROMOTION_FIELDS, self.promotion)
        write_csv(self.root / "Docs/ledgers/VERIFY_LEDGER.csv", gates.VERIFY_FIELDS, self.verify)
        write_csv(self.root / "Docs/ledgers/GATE_SLICES.csv", gates.SLICE_FIELDS, self.slices)
        write_csv(self.root / "Docs/ledgers/EC_BRIDGES.csv", gates.BRIDGE_FIELDS, self.bridges)
        manifest = self.root / "tools/port/copy_manifest.json"
        manifest.write_text(json.dumps(self.manifest), encoding="utf-8")
        baseline = self.root / "tools/lint/baseline.json"
        baseline.parent.mkdir(parents=True, exist_ok=True)
        baseline.write_text(json.dumps({"schema": 1, "findings": self.baseline}), encoding="utf-8")
        self.write_project()
        workspace = gates.Workspace(self.root)
        gates.write_summary(workspace)
        return workspace

    def write_project(self) -> None:
        items = []
        for path in self.files:
            if not path.startswith("Native/Game/src/") or Path(path).suffix not in {".c", ".cpp"}:
                continue
            relative = path.removeprefix("Native/Game/").replace("/", "\\")
            if relative in self.relaxed:
                items.append(RELAXED_ITEM.format(path=relative))
            else:
                items.append(f'<ClCompile Include="{relative}" />')
        project = self.root / "Native/Game/VS.Game.vcxproj"
        project.parent.mkdir(parents=True, exist_ok=True)
        project.write_text(
            '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n'
            f"  <ItemGroup>\n    {chr(10).join(items)}\n  </ItemGroup>\n</Project>\n",
            encoding="utf-8",
        )


def owned_row(identifier: str, name: str, path: str, *, header: str = "") -> dict[str, str]:
    return {
        "id": identifier, "kind": "function", "name": name, "path": path,
        "header": header or path.rsplit(".", 1)[0] + ".h", "since": "2026-10-30", "notes": "",
    }


def promotion_row(**values: str) -> dict[str, str]:
    return {
        "id": "F00003", "path": r"Native\Game\src\Sys\Thing.cpp",
        "source_cpp": "x", "vs_namespace": "x", "typed_interface": "x", "no_bridge": "x",
        "strict_clean": "x", "behavior_evidence": "x",
        "evidence": "test:Thing adds one", "reviewed": REVISION, "notes": "", **values,
    }


def write_csv(path: Path, fields: list[str], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


class GateTestCase(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.fixture = Fixture(Path(directory.name))

    def rules(self, gate: str | None = None) -> set[str]:
        report = gates.build_report(self.fixture.write())
        return {item.rule for item in report.issues if gate is None or item.gate == gate}

    def report(self) -> gates.Report:
        return gates.build_report(self.fixture.write())


class OwnershipGateTests(GateTestCase):
    def test_consistent_fixture_passes_every_gate(self) -> None:
        self.assertEqual(self.rules(), set())

    def test_each_row_has_one_ownership_state(self) -> None:
        states = self.report().ownership_states
        self.assertEqual(states["F00001"], "generated")
        self.assertEqual(states["F00002"], "owned-c")
        self.assertEqual(states["F00003"], "owned-cpp")
        self.assertEqual(states["F00004"], "retired")
        self.assertEqual(states["F00005"], "excluded")

    def test_owned_id_must_exist_in_the_catalog(self) -> None:
        self.fixture.owned.append(owned_row("F09999", "Sys_Ghost", r"Native\Game\src\Sys\OwnedC.c"))
        self.assertIn("owned-id-missing", self.rules("ownership"))

    def test_duplicate_owned_id_is_rejected(self) -> None:
        self.fixture.owned.append(dict(self.fixture.owned[0]))
        self.assertIn("owned-id-duplicate", self.rules("ownership"))

    def test_missing_owned_path_or_header_is_rejected(self) -> None:
        del self.fixture.files["Native/Game/src/Sys/OwnedC.h"]
        self.assertIn("owned-path-missing", self.rules("ownership"))

    def test_owned_path_cannot_be_generated_output(self) -> None:
        self.fixture.manifest[r"Native\Game\src\Sys\OwnedC.c"] = "hash"
        self.assertIn("owned-path-generated", self.rules("ownership"))

    def test_catalog_location_must_equal_owned_path(self) -> None:
        self.fixture.catalog[1]["final_location"] = r"Native\Game\src\Sys\Elsewhere.c"
        self.assertIn("owned-location-mismatch", self.rules("ownership"))

    def test_unregistered_row_in_an_owned_file_is_rejected(self) -> None:
        self.fixture.catalog.append(
            catalog_row("F00006", final_location=r"Native\Game\src\Sys\OwnedC.c")
        )
        self.assertIn("unregistered-row-in-owned-file", self.rules("ownership"))

    def test_invalid_copied_value_is_rejected(self) -> None:
        self.fixture.catalog[0]["copied"] = "yes"
        self.assertIn("invalid-copied", self.rules("ownership"))

    def test_retired_row_requires_copy_no(self) -> None:
        self.fixture.catalog[3]["copy"] = "yes"
        self.assertIn("retired-without-evidence", self.rules("ownership"))

    def test_retired_row_requires_a_retired_by_note(self) -> None:
        self.fixture.catalog[3]["notes"] = "replaced"
        self.assertIn("retired-without-evidence", self.rules("ownership"))

    def test_retired_row_in_an_owned_file_is_not_missing_ownership(self) -> None:
        self.assertNotIn("unregistered-row-in-owned-file", self.rules("ownership"))

    def test_ownership_alone_never_implies_cpp_modernized_or_verified(self) -> None:
        report = self.report()
        self.assertEqual(report.ownership_states["F00002"], "owned-c")
        self.assertEqual(report.row_status["F00002"], {"cpp": "", "modernized": "", "verified": ""})
        self.assertEqual(report.ownership_states["F00003"], "owned-cpp")
        self.assertEqual(report.derived_cpp["F00003"], "wip")
        self.assertEqual(report.row_status["F00003"]["modernized"], "")
        self.assertEqual(report.row_status["F00003"]["verified"], "")

    def test_owned_cpp_function_requires_a_promotion_record(self) -> None:
        self.fixture.promotion = []
        self.fixture.catalog[2]["cpp"] = ""
        self.assertIn("promotion-record-missing", self.rules("promotion"))

    def test_claiming_status_from_ownership_alone_fails_each_separate_gate(self) -> None:
        self.fixture.catalog[2]["cpp"] = "x"
        self.fixture.catalog[2]["verified"] = "x"
        rules = self.rules()
        self.assertIn("cpp-mismatch", rules)
        self.assertIn("verified-without-record", rules)

    def test_summary_block_is_generated_and_stale_counts_fail(self) -> None:
        workspace = self.fixture.write()
        gates.write_summary(workspace)
        self.assertEqual(gates.build_report(workspace).issues, [])
        summary = workspace.summary.read_text(encoding="utf-8")
        workspace.summary.write_text(
            summary.replace("| 2 | owned functions", "| 195 | owned functions"), encoding="utf-8"
        )
        self.assertIn("summary-stale", {item.rule for item in gates.build_report(workspace).issues})


class PromotionGateTests(GateTestCase):
    def test_real_promotion_derives_cpp_x(self) -> None:
        self.fixture.promote()
        report = self.report()
        self.assertEqual(report.issues, [])
        self.assertEqual(report.derived_cpp["F00003"], "x")

    def expect_failure(self, step: str) -> None:
        report = self.report()
        self.assertIn(step, {item.rule for item in report.issues})
        self.assertNotEqual(report.derived_cpp["F00003"], "x")

    def test_c_source_cannot_claim_source_cpp(self) -> None:
        self.fixture.promote(path=r"Native\Game\src\Sys\OwnedC.c")
        self.fixture.catalog[2]["final_location"] = r"Native\Game\src\Sys\OwnedC.c"
        self.fixture.owned[1]["path"] = r"Native\Game\src\Sys\OwnedC.c"
        self.expect_failure("claim-source_cpp")

    def test_relaxed_compilation_cannot_claim_source_cpp(self) -> None:
        self.fixture.promote()
        self.fixture.relax("src\\Sys\\Thing.cpp")
        self.expect_failure("claim-source_cpp")

    def test_global_c_abi_symbol_cannot_claim_namespace(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            'extern "C" int Sys_Thing(void) { return 2; }\n'
        )
        self.expect_failure("claim-vs_namespace")

    def test_global_definition_of_the_short_name_cannot_claim_namespace(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = "int Thing(int v) { return v + 1; }\n"
        self.expect_failure("claim-vs_namespace")

    def test_anonymous_namespace_cannot_claim_namespace(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            "namespace VS::Sys {\nnamespace {\nint Thing(int v) { return v + 1; }\n}\n}\n"
        )
        self.expect_failure("claim-vs_namespace")

    def test_wrong_namespace_cannot_claim_namespace(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            "namespace VS::Other {\nint Thing(int v) { return v + 1; }\n}\n"
        )
        self.expect_failure("claim-vs_namespace")

    def test_declaration_alone_cannot_claim_namespace(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            "namespace VS::Sys {\nint Thing(int v);\n}\n"
        )
        self.expect_failure("claim-vs_namespace")

    def test_forwarding_wrapper_cannot_claim_namespace(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            'extern "C" int Sys_Thing(int v);\n'
            "namespace VS::Sys {\nint Thing(int v) { return Sys_Thing(v); }\n}\n"
        )
        self.expect_failure("claim-vs_namespace")

    def test_filename_change_alone_never_derives_cpp_x(self) -> None:
        report = self.report()
        self.assertNotEqual(report.derived_cpp.get("F00003", ""), "x")

    def test_nested_namespace_blocks_and_qualified_definitions_qualify(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            "namespace VS {\nnamespace Sys {\nint Thing(int v);\n}\n}\n"
            "int VS::Sys::Thing(int v) { return v + 1; }\n"
        )
        self.assertEqual(self.report().issues, [])

    def test_legacy_typedef_signature_cannot_claim_typed_interface(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            "namespace VS::Sys {\nu_int Thing(int v) { return v + 1; }\n}\n"
        )
        self.expect_failure("claim-typed_interface")

    def test_void_pointer_signature_cannot_claim_typed_interface(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            "namespace VS::Sys {\nint Thing(void* v) { return v != nullptr; }\n}\n"
        )
        self.expect_failure("claim-typed_interface")

    def test_blank_modernized_cannot_claim_no_bridge(self) -> None:
        self.fixture.promote()
        self.fixture.catalog[2]["modernized"] = ""
        self.expect_failure("claim-no_bridge")

    def test_legacy_include_cannot_claim_no_bridge(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = (
            '#include "Legacy/Psx.h"\n' + PROMOTED_SOURCE
        )
        self.expect_failure("claim-no_bridge")

    def test_baseline_debt_cannot_claim_strict_clean(self) -> None:
        self.fixture.promote()
        self.fixture.baseline.append(
            {"key": "k", "count": 1, "tool": "tidy", "rule": "r",
             "path": "Native/Game/src/Sys/Thing.cpp"}
        )
        self.expect_failure("claim-strict_clean")

    def test_missing_test_case_cannot_claim_behavior(self) -> None:
        self.fixture.promote(evidence="test:No such test")
        self.expect_failure("claim-behavior_evidence")

    def test_missing_reviewed_revision_cannot_claim_behavior(self) -> None:
        self.fixture.promote(reviewed="")
        self.expect_failure("claim-behavior_evidence")

    def test_unadopted_callers_block_cpp_x_even_with_complete_claims(self) -> None:
        self.fixture.promote()
        self.fixture.files["Native/Game/src/Gen/Generated.c"] = (
            "extern int Sys_Thing(void);\nint a(void) { return Sys_Thing(); }\n"
        )
        self.expect_failure("legacy-symbol-consumers")

    def test_multiline_test_case_names_resolve(self) -> None:
        self.fixture.promote(evidence="test:Thing keeps state")
        self.assertEqual(self.report().issues, [])

    def test_sync_seeds_records_and_derives_wip(self) -> None:
        self.fixture.promotion = []
        self.fixture.catalog[2]["cpp"] = ""
        workspace = self.fixture.write()
        gates.sync(workspace)
        rows = {row["id"]: row for row in gates.read_csv(workspace.promotion)}
        self.assertEqual(set(rows), {"F00003"})
        self.assertEqual(rows["F00003"]["source_cpp"], "x")
        catalog = {row["id"]: row for row in gates.read_csv(workspace.catalog)}
        self.assertEqual(catalog["F00003"]["cpp"], "wip")
        self.assertEqual(catalog["F00002"]["cpp"], "")
        self.assertEqual(gates.build_report(workspace).issues, [])

    def test_sync_corrects_an_overclaim_to_wip(self) -> None:
        self.fixture.catalog[2]["cpp"] = "x"
        workspace = self.fixture.write()
        gates.sync(workspace)
        catalog = {row["id"]: row for row in gates.read_csv(workspace.catalog)}
        self.assertEqual(catalog["F00003"]["cpp"], "wip")

    def test_sync_is_idempotent_and_preserves_human_claims(self) -> None:
        self.fixture.promote(notes="kept note", evidence="test:Thing adds one;extra:keep")
        workspace = self.fixture.write()
        gates.sync(workspace)
        first = workspace.promotion.read_bytes(), workspace.catalog.read_bytes()
        gates.sync(workspace)
        self.assertEqual((workspace.promotion.read_bytes(), workspace.catalog.read_bytes()), first)
        row = gates.read_csv(workspace.promotion)[0]
        self.assertEqual(row["notes"], "kept note")
        self.assertEqual(row["evidence"], "test:Thing adds one;extra:keep")
        self.assertEqual(row["typed_interface"], "x")

    def test_sync_preserves_catalog_line_endings_and_other_columns(self) -> None:
        self.fixture.catalog[0]["notes"] = 'quoted, "note"'
        workspace = self.fixture.write()
        before = {row["id"]: row for row in gates.read_csv(workspace.catalog)}
        gates.sync(workspace)
        after = {row["id"]: row for row in gates.read_csv(workspace.catalog)}
        self.assertEqual(after["F00001"], before["F00001"])
        self.assertNotIn(b"\r\n", workspace.catalog.read_bytes())

    def test_per_step_and_per_file_inventory_is_reported(self) -> None:
        self.fixture.promote()
        inventory = self.report().promotion_inventory
        self.assertEqual(inventory["steps"]["vs_namespace"], 1)
        self.assertEqual(inventory["files"][r"Native\Game\src\Sys\Thing.cpp"]["rows"], 1)


class QualityGateTests(GateTestCase):
    def source_class(self, path: str) -> str:
        return self.report().source_classes[path]["class"]

    def test_relaxed_owned_cpp_is_rejected(self) -> None:
        self.fixture.relax("src\\Sys\\Thing.cpp")
        self.assertIn("owned-cpp-relaxed", self.rules("quality"))

    def test_owned_c_is_reported_as_maintained_relaxed_debt(self) -> None:
        report = self.report()
        entry = report.source_classes["Native/Game/src/Sys/OwnedC.c"]
        self.assertEqual(entry["class"], "maintained-c")
        self.assertEqual(entry["compile"], "relaxed")
        self.assertEqual(report.relaxation_debt[0]["path"], "Native/Game/src/Sys/OwnedC.c")
        self.assertEqual(report.relaxation_debt[0]["owned_functions"], 1)

    def test_owned_relaxed_replacement_code_stays_on_the_debt_list(self) -> None:
        report = gates.build_report(self.fixture.write(), retiring={"Native/Game/src/Sys/OwnedC.c": "EC-02"})
        entry = report.source_classes["Native/Game/src/Sys/OwnedC.c"]
        self.assertEqual(entry["class"], "replacement-legacy")
        self.assertEqual([item["path"] for item in report.relaxation_debt], ["Native/Game/src/Sys/OwnedC.c"])
        self.assertEqual(report.relaxation_debt[0]["class"], "replacement-legacy")

    def test_unowned_relaxed_legacy_code_is_not_maintained_debt(self) -> None:
        self.fixture.files["Native/Game/src/Sys/Shim.c"] = "int shim;\n"
        self.fixture.relax("src\\Sys\\Shim.c")
        report = gates.build_report(self.fixture.write(), retiring={"Native/Game/src/Sys/Shim.c": "EC-02"})
        self.assertNotIn("Native/Game/src/Sys/Shim.c", [item["path"] for item in report.relaxation_debt])

    def test_generated_output_is_outside_quality_scope(self) -> None:
        report = self.report()
        entry = report.source_classes["Native/Game/src/Gen/Generated.c"]
        self.assertEqual(entry["class"], "generated-c")
        self.assertFalse(entry["quality_scope"])

    def test_strict_cpp_comes_from_project_settings_not_the_extension(self) -> None:
        self.assertEqual(self.report().source_classes["Native/Game/src/Sys/Thing.cpp"]["compile"], "strict")
        self.fixture.relax("src\\Sys\\Thing.cpp")
        self.assertEqual(self.report().source_classes["Native/Game/src/Sys/Thing.cpp"]["compile"], "relaxed")

    def test_relaxed_project_defaults_make_every_entry_relaxed(self) -> None:
        self.fixture.files["Directory.Build.props"] = STRICT_PROPS.replace("Level4", "Level1")
        self.assertEqual(self.report().source_classes["Native/Game/src/Sys/Thing.cpp"]["compile"], "relaxed")

    def test_replacement_and_legacy_code_is_a_separate_class(self) -> None:
        self.fixture.files["Native/Game/src/Sys/Scratch.cpp"] = "int s;\n"
        report = gates.build_report(self.fixture.write(), retiring={"Native/Game/src/Sys/Scratch.cpp": "EC-05"})
        self.assertEqual(report.source_classes["Native/Game/src/Sys/Scratch.cpp"]["class"], "replacement-legacy")

    def test_baseline_debt_is_counted_per_file(self) -> None:
        self.fixture.baseline += [
            {"key": "a", "count": 2, "tool": "tidy", "rule": "r", "path": "Native/Game/src/Sys/OwnedC.c"},
            {"key": "b", "count": 1, "tool": "tidy", "rule": "s", "path": "Native/Game/src/Sys/OwnedC.c"},
        ]
        self.assertEqual(self.report().relaxation_debt[0]["baseline_findings"], 3)

    def test_relaxed_promotion_claim_is_rejected_by_the_quality_gate(self) -> None:
        self.fixture.promote()
        self.fixture.relax("src\\Sys\\Thing.cpp")
        self.assertIn("promotion-claim-relaxed", self.rules("quality"))

    def test_strict_clean_claim_with_debt_is_rejected_by_the_quality_gate(self) -> None:
        self.fixture.promote()
        self.fixture.baseline.append(
            {"key": "k", "count": 1, "tool": "tidy", "rule": "r",
             "path": "Native/Game/src/Sys/Thing.cpp"}
        )
        self.assertIn("strict-clean-with-debt", self.rules("quality"))

    def test_gate_tool_never_modifies_the_baseline(self) -> None:
        self.fixture.baseline.append(
            {"key": "k", "count": 1, "tool": "tidy", "rule": "r",
             "path": "Native/Game/src/Sys/OwnedC.c"}
        )
        workspace = self.fixture.write()
        before = hashlib.sha256(workspace.baseline.read_bytes()).hexdigest()
        gates.build_report(workspace)
        gates.sync(workspace)
        gates.write_summary(workspace)
        gates.write_reports(gates.build_report(workspace), self.fixture.root / "reports")
        self.assertEqual(hashlib.sha256(workspace.baseline.read_bytes()).hexdigest(), before)


def verify_record(**values: str) -> dict[str, str]:
    return {
        "record": "VR-0001", "ids": "F00001", "kind": "unit-test",
        "references": "test:Thing adds one", "revision": REVISION,
        "configurations": "Debug;Release", "command": "VS.Tests.exe", "dirty": "clean",
        "expected": "adds one", "outcome": "pass", "limits": "component only",
        "notes": "", **values,
    }


class BehaviorGateTests(GateTestCase):
    def setUp(self) -> None:
        super().setUp()
        self.fixture.catalog[0]["verified"] = "x"
        self.fixture.verify = [verify_record()]

    def test_marker_with_a_resolving_record_passes(self) -> None:
        self.assertEqual(self.rules("behavior"), set())

    def test_marker_without_a_record_is_rejected(self) -> None:
        self.fixture.verify = []
        self.assertIn("verified-without-record", self.rules("behavior"))

    def test_unresolved_test_reference_is_rejected(self) -> None:
        self.fixture.verify[0]["references"] = "test:Not a real test"
        self.assertIn("unresolved-reference", self.rules("behavior"))

    def test_unresolved_replay_reference_is_rejected(self) -> None:
        self.fixture.verify[0]["references"] = "replay:tools/test/missing.txt"
        self.assertIn("unresolved-reference", self.rules("behavior"))

    def test_record_needs_a_checkable_reference(self) -> None:
        self.fixture.verify[0]["references"] = "issue:#18"
        self.assertIn("no-checkable-reference", self.rules("behavior"))

    def test_missing_revision_is_rejected(self) -> None:
        self.fixture.verify[0]["revision"] = ""
        self.assertIn("invalid-revision", self.rules("behavior"))

    def test_missing_required_configuration_is_rejected(self) -> None:
        self.fixture.verify[0]["configurations"] = "Debug"
        self.assertIn("missing-configuration", self.rules("behavior"))

    def test_single_configuration_is_enough_for_a_device_record(self) -> None:
        self.fixture.verify[0].update(kind="device", configurations="Release")
        self.assertEqual(self.rules("behavior"), set())

    def test_invalid_marker_value_is_rejected(self) -> None:
        self.fixture.catalog[0]["verified"] = "yes"
        self.assertIn("invalid-verified", self.rules("behavior"))

    def test_missing_limits_are_rejected(self) -> None:
        self.fixture.verify[0]["limits"] = ""
        self.assertIn("missing-limits", self.rules("behavior"))

    def test_failed_record_does_not_support_a_marker(self) -> None:
        self.fixture.verify[0]["outcome"] = "fail"
        self.assertIn("verified-without-record", self.rules("behavior"))

    def test_record_covering_an_unknown_id_is_rejected(self) -> None:
        self.fixture.verify[0]["ids"] = "F00001;F09999"
        self.assertIn("record-unknown-id", self.rules("behavior"))

    def test_invalid_kind_is_rejected(self) -> None:
        self.fixture.verify[0]["kind"] = "vibes"
        self.assertIn("invalid-kind", self.rules("behavior"))

    def test_promotion_links_to_a_verification_record(self) -> None:
        self.fixture.promote(evidence="verify:VR-0001")
        self.fixture.verify[0]["ids"] = "F00001;F00003"
        self.assertEqual(self.report().issues, [])

    def test_promotion_link_must_cover_the_row(self) -> None:
        self.fixture.promote(evidence="verify:VR-0001")
        self.assertIn("claim-behavior_evidence", self.rules("promotion"))


class RetirementGateTests(GateTestCase):
    def test_controls_pass_with_owned_wip(self) -> None:
        self.fixture.catalog[1]["modernized"] = "wip"
        self.assertEqual(self.rules("retirement"), set())

    def test_modernized_x_requires_ownership(self) -> None:
        self.fixture.catalog[0]["modernized"] = "x"
        self.assertIn("modernized-not-owned", self.rules("retirement"))

    def test_modernized_wip_requires_ownership(self) -> None:
        self.fixture.catalog[0]["modernized"] = "wip"
        self.assertIn("modernized-not-owned", self.rules("retirement"))

    def test_modernized_x_with_a_direct_bridge_include_is_rejected(self) -> None:
        self.fixture.catalog[2]["modernized"] = "x"
        self.fixture.files["Native/Game/src/Sys/Thing.cpp"] = '#include "Core/Gte.h"\n'
        self.assertIn("modernized-with-bridge", self.rules("retirement"))

    def test_modernized_x_with_an_indirect_bridge_include_is_rejected(self) -> None:
        self.fixture.catalog[2]["modernized"] = "x"
        self.fixture.files["Native/Game/src/Sys/Thing.hpp"] = '#include "Legacy/Psx.h"\n'
        self.assertIn("modernized-with-bridge", self.rules("retirement"))

    def test_invalid_modernized_value_is_rejected(self) -> None:
        self.fixture.catalog[1]["modernized"] = "done"
        self.assertIn("invalid-modernized", self.rules("retirement"))

    def test_consumers_are_counted_per_ec_by_ownership(self) -> None:
        self.fixture.files["Native/Game/src/Sys/OwnedC.c"] = '#include "Core/Gte.h"\nint Sys_OwnedC(void) { return 1; }\n'
        entry = self.report().bridge_consumers["EC-03"]
        self.assertEqual(entry["direct"]["owned"], ["Native/Game/src/Sys/OwnedC.c"])
        self.assertEqual(entry["transitive"]["generated"], ["Native/Game/src/Gen/Generated.c"])

    def test_legacy_sdk_consumers_are_reported(self) -> None:
        legacy = self.report().legacy_consumers["Native/Engine/Legacy/Psx.h"]
        self.assertEqual(legacy["generated"], ["Native/Game/src/Gen/Generated.c"])
        self.assertEqual(legacy["owned"], ["Native/Game/src/Sys/OwnedC.c"])

    def test_renamed_bridge_listed_in_the_map_keeps_its_consumer_count(self) -> None:
        self.fixture.files["Native/Game/src/Sys/OwnedC.c"] = '#include "Core/Gte.h"\nint Sys_OwnedC(void) { return 1; }\n'
        before = self.report().bridge_consumers["EC-03"]
        del self.fixture.files["Native/Engine/include/Core/Gte.h"]
        self.fixture.files["Native/Engine/include/Modern/Transform.h"] = "#pragma once\n"
        self.fixture.files["Native/Engine/src/Core/Gte.cpp"] = '#include "Modern/Transform.h"\n'
        self.fixture.files["Native/Engine/Legacy/Psx.h"] = '#include "Modern/Transform.h"\n'
        self.fixture.files["Native/Game/src/Sys/OwnedC.c"] = (
            '#include "Modern/Transform.h"\nint Sys_OwnedC(void) { return 1; }\n'
        )
        self.fixture.bridges[0]["path"] = "Native/Engine/include/Modern/Transform.h"
        after = self.report().bridge_consumers["EC-03"]
        self.assertEqual(after["direct"], before["direct"])
        self.assertEqual(after["transitive"], before["transitive"])

    def test_bridge_renamed_without_updating_the_map_fails_instead_of_dropping_to_zero(self) -> None:
        del self.fixture.files["Native/Engine/include/Core/Gte.h"]
        self.fixture.files["Native/Engine/include/Modern/Transform.h"] = "#pragma once\n"
        self.assertIn("bridge-path-missing", self.rules("retirement"))

    def test_slice_after_count_must_match_the_measured_consumers(self) -> None:
        self.fixture.slices = [slice_record(after="0")]
        self.fixture.files["Native/Game/src/Gen/Generated.c"] = "int a(void) { return Sys_OldBridge(); }\n"
        self.assertIn("slice-consumer-mismatch", self.rules("retirement"))

    def test_slice_with_zero_consumers_passes_and_reports_before_and_after(self) -> None:
        self.fixture.slices = [slice_record(after="0")]
        report = self.report()
        self.assertEqual(report.issues, [])
        self.assertEqual(report.slices[0]["before"], 4)
        self.assertEqual(report.slices[0]["after"], 0)

    def test_slice_cannot_cover_an_unowned_row(self) -> None:
        self.fixture.slices = [slice_record(ids="F00001", after="0")]
        self.assertIn("slice-not-owned", self.rules("retirement"))


def slice_record(**values: str) -> dict[str, str]:
    return {
        "slice": "S1", "ids": "F00003", "old_bridges": "symbol:Sys_OldBridge",
        "ec": "EC-04", "before": "4", "after": "0", "before_revision": REVISION,
        "evidence": "report", "notes": "", **values,
    }


if __name__ == "__main__":
    unittest.main()
