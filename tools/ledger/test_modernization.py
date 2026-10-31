"""Synthetic controls for the modernization work ledger; no disc data or live ledgers."""

from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import modernization  # noqa: E402

CATALOG_FIELDS = [
    "id", "name", "copy", "final_name", "final_location", "lines", "copied", "cpp", "verified",
    "scratchpad", "kseg_literal", "ot24", "ptr_cast", "inline_asm", "sdk_apis", "notes",
]
OWNED_FIELDS = ["id", "kind", "name", "path", "header", "since", "notes"]
PROMOTION_FIELDS = [
    "id", "path", "source_cpp", "vs_namespace", "typed_interface", "no_bridge", "strict_clean",
    "behavior_evidence", "evidence", "reviewed", "notes",
]
VERIFY_FIELDS = [
    "record", "ids", "kind", "references", "revision", "configurations", "command", "dirty", "expected",
    "outcome", "limits", "notes",
]
PROJECT = """<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup>
<ClCompile Include="src\\Battle\\Ai\\Plan.cpp" />
<ClCompile Include="src\\Battle\\Owned.cpp" />
<ClCompile Include="src\\Boot\\ModulePointers.cpp" />
<ClCompile Include="src\\Audio\\Mix.cpp"><WarningLevel>Level1</WarningLevel></ClCompile>
</ItemGroup></Project>
"""
GENERATED = "Native/Game/src/Battle/Ai/Plan.cpp"
OWNED = "Native/Game/src/Battle/Owned.cpp"
AUDIO = "Native/Game/src/Audio/Mix.cpp"


def catalog_row(identifier: str, final_name: str, location: str, **values: str) -> dict[str, str]:
    row = dict.fromkeys(CATALOG_FIELDS, "")
    row.update(
        id=identifier, name=f"func_{identifier}", copy="yes", final_name=final_name,
        final_location=location.replace("/", "\\"), lines="10", copied="x",
    )
    row.update(values)
    return row


def write_csv(path: Path, fields: list[str], rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        writer.writerows(rows)


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.catalog = [
            catalog_row("F00001", "Battle::Ai::Plan", GENERATED, sdk_apis="CdRead", scratchpad="1"),
            catalog_row("F00002", "Battle::Owned", OWNED, cpp="wip"),
            catalog_row("F00003", "Battle::Done", OWNED, cpp="x", verified="x"),
            catalog_row("F00004", "Audio::Mix", AUDIO),
            catalog_row("F00005", "Core::Math::Alias", AUDIO, copied="stub"),
            catalog_row("F00006", "Gone::Excluded", "", copy="no"),
        ]
        self.owned = [
            {"id": "F00002", "kind": "function", "name": "Battle_Owned", "path": OWNED.replace("/", "\\"),
             "header": "", "since": "", "notes": ""},
            {"id": "F00003", "kind": "function", "name": "VS::Battle::Done", "path": OWNED.replace("/", "\\"),
             "header": "", "since": "", "notes": ""},
        ]
        self.promotion = [
            {**dict.fromkeys(PROMOTION_FIELDS, ""), "id": "F00002", "path": OWNED.replace("/", "\\"),
             "source_cpp": "x", "strict_clean": "x"},
            {**dict.fromkeys(PROMOTION_FIELDS, ""), "id": "F00003", "path": OWNED.replace("/", "\\"),
             "source_cpp": "x", "vs_namespace": "x", "typed_interface": "x", "no_bridge": "x",
             "strict_clean": "x", "behavior_evidence": "x"},
        ]
        self.verify = [{**dict.fromkeys(VERIFY_FIELDS, ""), "record": "VR-F00003", "ids": "F00003",
                        "kind": "unit-test", "outcome": "pass"}]
        self.files = {
            GENERATED: 'void Battle_Ai_Plan(void* work) { CdRead(work); VS_UNPORTED("not-a-shim"); }\n',
            OWNED: "#define Local_Thing Other\nvoid Battle_Owned(int value) { Battle_Ai_Plan(0); }\n"
                   "namespace VS::Battle { void Done() { Battle_Owned(1); } }\n",
            AUDIO: "void Audio_Mix(int value) {}\nint Target(int value) { return value; }\n",
            "Native/Game/src/Audio/Math.h": "#define Core_Math_Alias Target\n",
            "Native/Game/src/Boot/ModulePointers.cpp": "void* table[] = {&Battle_Ai_Plan, &Audio_Mix};\n",
            "Native/Engine/Legacy/LibCd.cpp": 'int CdRead(void* work) { VS_UNPORTED("libcd:CdRead"); return 0; }\n'
                                              'void CdSync(void) { VS_UNPORTED("libcd:CdSync"); }\n',
            "Native/Tests/src/PlanTests.cpp": "TEST_CASE(\"plan\") { Battle_Ai_Plan(0); Battle_Owned(1); }\n",
        }
        self.manifest = {
            GENERATED.replace("/", "\\"): "hash", "Native\\Game\\src\\Boot\\ModulePointers.cpp": "hash",
            AUDIO.replace("/", "\\"): "hash",
        }
        self.tracker = (
            "# Tracker\n\n" + modernization.SUMMARY_BEGIN + "\n" + modernization.SUMMARY_END + "\n"
        )

    def write(self) -> modernization.ModernizationWorkspace:
        ledgers = self.root / "Docs" / "ledgers"
        write_csv(ledgers / "FUNCTION_CATALOG.csv", CATALOG_FIELDS, self.catalog)
        write_csv(ledgers / "CPP_PROMOTION.csv", PROMOTION_FIELDS, self.promotion)
        write_csv(ledgers / "VERIFY_LEDGER.csv", VERIFY_FIELDS, self.verify)
        write_csv(self.root / "tools" / "port" / "owned.csv", OWNED_FIELDS, self.owned)
        (self.root / "tools" / "port" / "copy_manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        for path, text in self.files.items():
            full = self.root / path
            full.parent.mkdir(parents=True, exist_ok=True)
            full.write_text(text, encoding="utf-8")
        (self.root / "Native" / "Game" / "VS.Game.vcxproj").write_text(PROJECT, encoding="utf-8")
        tracker = self.root / "Docs" / "MODERNIZATION.md"
        if not tracker.exists():
            tracker.write_text(self.tracker, encoding="utf-8")
        return modernization.ModernizationWorkspace(self.root)


def coverage(**functions: int) -> dict:
    return {"data": [{"functions": [
        {"name": name, "count": count, "regions": [[1, 1, 2, 2, count, 0, 0, 0], [3, 1, 4, 2, 0, 0, 0, 0]]}
        for name, count in functions.items()
    ]}]}


class ModernizationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.fixture = Fixture(Path(self.directory.name))
        self.workspace = self.fixture.write()

    def rows(self) -> dict[str, dict[str, str]]:
        return {row["id"]: row for row in modernization.read_csv(self.workspace.ledger)}

    def rules(self) -> set[str]:
        return {issue.rule for issue in modernization.check(self.workspace)}

    def test_sync_lists_every_port_target_once_and_no_excluded_row(self) -> None:
        modernization.sync(self.workspace)
        self.assertEqual(sorted(self.rows()), ["F00001", "F00002", "F00003", "F00004", "F00005"])

    def test_tracks_separate_owned_and_generated(self) -> None:
        modernization.sync(self.workspace)
        rows = self.rows()
        self.assertEqual(rows["F00001"]["track"], "generated")
        self.assertEqual(rows["F00002"]["track"], "owned")

    def test_owned_steps_come_from_the_promotion_ledger(self) -> None:
        modernization.sync(self.workspace)
        rows = self.rows()
        self.assertEqual(rows["F00002"]["strict_clean"], "x")
        self.assertEqual(rows["F00002"]["vs_namespace"], "")
        self.assertEqual(rows["F00001"]["strict_clean"], "")

    def test_only_a_promoted_function_starts_done(self) -> None:
        modernization.sync(self.workspace)
        rows = self.rows()
        self.assertEqual(rows["F00003"]["status"], "done")
        self.assertEqual(rows["F00002"]["status"], "todo")

    def test_consumers_are_split_and_exclude_the_definition_and_module_table(self) -> None:
        modernization.sync(self.workspace)
        plan = self.rows()["F00001"]
        self.assertEqual(plan["hand_consumers"], "1")
        self.assertEqual(plan["test_consumers"], "1")
        self.assertEqual(plan["generated_consumers"], "0")
        self.assertEqual(plan["module_table"], "x")
        self.assertEqual(self.rows()["F00002"]["generated_consumers"], "0")

    def test_blockers_are_measured_from_the_native_body(self) -> None:
        modernization.sync(self.workspace)
        plan = self.rows()["F00001"]
        self.assertEqual(plan["legacy_calls"], "CdRead")
        self.assertEqual(plan["legacy_signature"], "x")
        self.assertEqual(plan["source_flags"], "scratchpad")
        self.assertEqual(self.rows()["F00004"]["legacy_signature"], "")

    def test_compile_mode_comes_from_the_project(self) -> None:
        modernization.sync(self.workspace)
        self.assertEqual(self.rows()["F00004"]["compile"], "relaxed")
        self.assertEqual(self.rows()["F00002"]["compile"], "strict")

    def test_a_macro_alias_is_reported_as_implemented_by_its_target(self) -> None:
        modernization.sync(self.workspace)
        self.assertEqual(self.rows()["F00005"]["body"], "alias:Target")

    def add_template(self) -> None:
        self.fixture.catalog.append(catalog_row(
            "F00010", "Effects::Templates::Ring", "Native/Game/src/Effects/Ring.cpp", name="VS_RING_FUNCTION"))
        self.fixture.catalog.append(catalog_row("F00011", "Effects::Plg1::Ring", "Native/Game/src/Effects/Ring.cpp"))
        self.fixture.files["Native/Game/src/Effects/Ring.inl"] = (
            "void VS_RING_FUNCTION(void* work) { CdRead(work); }\n")
        self.fixture.files["Native/Game/src/Effects/Ring.cpp"] = (
            '#define VS_RING_FUNCTION Effects_Plg1_Ring\n#include "Effects/Ring.inl"\n#undef VS_RING_FUNCTION\n')
        self.fixture.write()

    def test_a_template_and_its_instances_share_one_measured_body(self) -> None:
        self.add_template()
        modernization.sync(self.workspace)
        rows = self.rows()
        self.assertEqual(rows["F00010"]["body"], "template:VS_RING_FUNCTION")
        self.assertEqual(rows["F00011"]["body"], "instance:VS_RING_FUNCTION")
        self.assertEqual(rows["F00011"]["legacy_calls"], "CdRead")
        self.assertEqual(rows["F00011"]["legacy_signature"], "x")

    def test_a_template_counts_as_executed_when_any_instance_ran(self) -> None:
        self.add_template()
        modernization.sync(self.workspace, coverage(Effects_Plg1_Ring=2))
        rows = self.rows()
        self.assertEqual(rows["F00010"]["tests_execute"], "yes")
        self.assertEqual(rows["F00011"]["tests_execute"], "yes")

    def test_replay_coverage_alone_counts_as_executed(self) -> None:
        modernization.sync(self.workspace, coverage(Audio_Mix=0), coverage(Audio_Mix=4))
        row = self.rows()["F00004"]
        self.assertEqual((row["tests_execute"], row["replay_execute"], row["regression"]), ("no", "yes", "executed"))

    def test_a_c_style_tagged_return_type_is_still_a_function(self) -> None:
        self.fixture.catalog.append(catalog_row("F00012", "Audio::Tagged", AUDIO))
        self.fixture.files[AUDIO] += "enum Signal Audio_Tagged(enum Group type) { return Ready; }\n"
        self.fixture.write()
        modernization.sync(self.workspace)
        self.assertEqual(self.rows()["F00012"]["body"], "found")

    def test_a_row_with_no_definition_is_reported_missing(self) -> None:
        self.fixture.catalog.append(catalog_row("F00007", "Audio::Nothing", AUDIO))
        self.fixture.write()
        modernization.sync(self.workspace)
        self.assertEqual(self.rows()["F00007"]["body"], "missing")

    def test_roadmap_mapping_uses_the_most_specific_path(self) -> None:
        self.assertEqual(modernization.roadmap_for(r"Native\Game\src\Battle\Ai\Plan.cpp")[0], "57")
        self.assertEqual(modernization.roadmap_for(r"Native\Game\src\Battle\Hud.cpp")[0], "56")

    def test_a_path_no_issue_covers_says_so(self) -> None:
        self.assertEqual(modernization.roadmap_for(r"Native\Game\src\Unmapped\Thing.cpp")[0], "none")

    def test_audio_belongs_to_its_roadmap_issue(self) -> None:
        self.assertEqual(modernization.roadmap_for(r"Native\Engine\src\Audio\Voice.cpp"), ("253", "30A"))

    def test_a_lettered_step_sorts_after_its_number(self) -> None:
        steps = sorted(["31", "30A", "", "3", "30"], key=modernization.step_order)
        self.assertEqual(steps, ["3", "30", "30A", "31", ""])

    def test_regression_is_unmeasured_until_coverage_arrives(self) -> None:
        modernization.sync(self.workspace)
        self.assertEqual(self.rows()["F00001"]["regression"], "unmeasured")
        self.assertEqual(self.rows()["F00003"]["regression"], "recorded")

    def test_coverage_separates_executed_from_characterize_first(self) -> None:
        modernization.sync(self.workspace, coverage(Battle_Ai_Plan=5, Audio_Mix=0))
        rows = self.rows()
        self.assertEqual(rows["F00001"]["tests_execute"], "yes")
        self.assertEqual(rows["F00001"]["region_pct"], "50")
        self.assertEqual(rows["F00001"]["regression"], "executed")
        self.assertEqual(rows["F00004"]["tests_execute"], "no")
        self.assertEqual(rows["F00004"]["regression"], "characterize-first")

    def test_coverage_understands_static_and_mangled_names_and_aliases(self) -> None:
        names = {"Native/Game/src/Audio/Mix.cpp:Audio_Mix": 1, "?Done@Battle@VS@@YAXXZ": 2, "Target": 3}
        modernization.sync(self.workspace, coverage(**names))
        rows = self.rows()
        self.assertEqual(rows["F00004"]["tests_execute"], "yes")
        self.assertEqual(rows["F00003"]["tests_execute"], "yes")
        self.assertEqual(rows["F00005"]["tests_execute"], "yes")

    def test_a_later_sync_keeps_coverage_and_hand_fields(self) -> None:
        modernization.sync(self.workspace, coverage(Battle_Ai_Plan=5))
        self.set_hand("F00001", status="in-progress", notes="slice one")
        modernization.sync(self.workspace)
        plan = self.rows()["F00001"]
        self.assertEqual(plan["status"], "in-progress")
        self.assertEqual(plan["notes"], "slice one")
        self.assertEqual(plan["tests_execute"], "yes")

    def set_hand(self, identifier: str, **values: str) -> None:
        rows = modernization.read_csv(self.workspace.ledger)
        for row in rows:
            if row["id"] == identifier:
                row.update(values)
        write_csv(self.workspace.ledger, modernization.FIELDS, rows)
        modernization.write_tracker(self.workspace, rows)

    def test_a_synced_ledger_passes(self) -> None:
        modernization.sync(self.workspace)
        self.assertEqual(self.rules(), set())

    def test_check_rejects_a_missing_ledger(self) -> None:
        self.assertIn("ledger-missing", self.rules())

    def test_check_rejects_a_new_catalog_row(self) -> None:
        modernization.sync(self.workspace)
        self.fixture.catalog.append(catalog_row("F00008", "Audio::Mix", AUDIO))
        self.fixture.write()
        self.assertIn("ledger-stale", self.rules())

    def test_check_rejects_a_function_that_moved(self) -> None:
        modernization.sync(self.workspace)
        self.fixture.catalog[0]["final_location"] = r"Native\Game\src\Battle\Moved.cpp"
        self.fixture.write()
        self.assertIn("ledger-stale", self.rules())

    def test_check_tolerates_measured_columns_drifting_between_syncs(self) -> None:
        modernization.sync(self.workspace)
        self.fixture.files[OWNED] += "void Extra() { Battle_Ai_Plan(0); }\n"
        self.fixture.write()
        self.assertEqual(self.rules(), set())

    def test_check_rejects_an_unknown_status(self) -> None:
        modernization.sync(self.workspace)
        self.set_hand("F00001", status="maybe")
        self.assertIn("status-unknown", self.rules())

    def test_work_cannot_start_without_regression_cover(self) -> None:
        modernization.sync(self.workspace, coverage(Audio_Mix=0))
        self.set_hand("F00004", status="in-progress")
        self.assertIn("work-without-regression", self.rules())

    def test_unmeasured_coverage_also_blocks_starting_work(self) -> None:
        modernization.sync(self.workspace)
        self.set_hand("F00001", status="in-progress")
        self.assertIn("work-without-regression", self.rules())

    def test_executed_code_may_start(self) -> None:
        modernization.sync(self.workspace, coverage(Battle_Ai_Plan=1))
        self.set_hand("F00001", status="in-progress")
        self.assertEqual(self.rules(), set())

    def test_done_requires_catalog_promotion(self) -> None:
        modernization.sync(self.workspace, coverage(Battle_Ai_Plan=1))
        self.set_hand("F00001", status="done")
        self.assertIn("done-not-promoted", self.rules())

    def test_catalog_promotion_requires_done(self) -> None:
        modernization.sync(self.workspace)
        self.set_hand("F00003", status="todo")
        self.assertIn("promoted-not-done", self.rules())

    def test_tracker_block_must_match_the_ledger(self) -> None:
        modernization.sync(self.workspace)
        rows = modernization.read_csv(self.workspace.ledger)
        rows[0]["lines"] = "9999"
        write_csv(self.workspace.ledger, modernization.FIELDS, rows)
        self.assertIn("tracker-stale", self.rules())

    def test_tracker_reports_unassigned_functions_and_shim_sites(self) -> None:
        self.fixture.catalog.append(catalog_row("F00009", "Audio::Other", "Native/Engine/src/Audio/Voice.cpp"))
        self.fixture.write()
        modernization.sync(self.workspace)
        text = self.workspace.tracker.read_text(encoding="utf-8")
        self.assertIn("no roadmap issue", text)
        self.assertIn("`libcd:CdRead`", text)
        self.assertNotIn("not-a-shim", text)

    def test_shim_sites_are_counted_only_in_legacy_sources(self) -> None:
        sites = modernization.shim_sites(modernization.gates.Sources(self.workspace))
        self.assertEqual(sites, {"Native/Engine/Legacy/LibCd.cpp": ["libcd:CdRead", "libcd:CdSync"]})


if __name__ == "__main__":
    unittest.main()
