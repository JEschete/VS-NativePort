"""Unit tests for promote.py on synthetic copies of the files it edits (#340).

  python tools\\port\\test_promote.py
"""

from __future__ import annotations

import ast
import contextlib
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import promote

FIXUPS = '''"""Synthetic copy_fixups.py."""

NATIVE_HELPER_HEADERS = {
    "VS::Battle::Ai::ApplyLiveActorAiSettings": r"Native\\Game\\include\\Battle\\Ai\\ActorSettingsLive.hpp",
    "VS::Field::OpenDoor": r"Native\\Game\\include\\Field\\Doors.hpp",
}

OWNED_CALL_BINDINGS = {}
OWNED_CALL_BINDINGS["F01813"] = "VS::Battle::Ai::ApplyLiveActorAiSettings"
OWNED_CALL_BINDINGS["F00828"] = "Field_RefreshAshleyInteractionTileEntry"

MODULE_BINDING_ENTRIES = [
    ("BATTLE/BATTLE.PRG", 0x800E5A9C, "VS::Battle::Ai::ApplyLiveActorAiSettings"),  # F01813
    ("BATTLE/BATTLE.PRG", 0x8009E634, "Field_RefreshAshleyInteractionTileEntry"),
]

FIXES = {
    # The door fix stays.
    "F00100": [
        ("old", """
int Door(void) {
}
"""),
    ],
    # F01818 keeps its callers' locals in registers.
    "F01818": [
        ("old", """
void Create(void) {
    int x;
}
"""),
    ],
    "F01900": [],
}
'''

SOLUTION = '''def c_headers(project, path):
    return (
            project == "VS.Engine"
            or project == "VS.Game" and path in (
                r"src\\Battle\\Ai\\ActorSettingsLive.cpp",
                r"src\\Field\\DoorsLive.cpp",
            ))
'''

OWNED = '''id,kind,name,path,header,since,notes
F01813,function,VS::Battle::Ai::ApplyActorAiSettings,Native\\Game\\src\\Battle\\Ai\\ActorSettings.cpp,Native\\Game\\include\\Battle\\Ai\\ActorSettings.hpp,2026-01-02,settings
F00920,function,VS::Field::FindPlatform,Native\\Game\\src\\Field\\Platforms.cpp,Native\\Game\\include\\Field\\Platforms.hpp,2025-12-01,legacy, unquoted, notes
'''

CATALOG_FIELDS = ("id,overlay,unit,file,line,name,kind,generic,lines,proposed_system,basis,copy,callers,callees,"
                  "sdk_apis,hw_reg,scratchpad,kseg_literal,ot24,ptr_cast,inline_asm,final_name,final_location,"
                  "purpose,copied,compiles,named,understood,modernized,cpp,verified,notes")


def catalog_row(identifier: str, address: int, notes: str = "") -> str:
    values = {"id": identifier, "overlay": "BATTLE.PRG", "unit": "BATTLE", "file": "BATTLE/BATTLE.PRG/7CD68.c",
              "line": "934", "name": f"func_{address:08X}", "kind": "C", "final_name": f"Battle::Ai::{identifier}",
              "final_location": r"Native\Game\src\Battle\Ai\ActorState.cpp", "copied": "x", "compiles": "x",
              "notes": notes}
    return ",".join(values.get(field, "") for field in CATALOG_FIELDS.split(","))


CATALOG = "\n".join([
    CATALOG_FIELDS,
    catalog_row("F01813", 0x800E5A9C),
    catalog_row("F01818", 0x800E6178, "conf:high"),
    catalog_row("F01819", 0x800E65DC),
    catalog_row("F01814", 0x800E5EC0),
    catalog_row("F00001", 0x80010000).replace("BATTLE/BATTLE.PRG/7CD68.c", "SLUS_010.40/1234.c"),
]) + "\n"

PROMOTION = '''id,path,source_cpp,vs_namespace,typed_interface,no_bridge,strict_clean,behavior_evidence,evidence,reviewed,notes
F01813,Native\\Game\\src\\Battle\\Ai\\ActorSettings.cpp,x,x,x,x,x,x,verify:VR-OLD,abc,old
F01900,Native\\Game\\src\\Battle\\Ai\\Later.cpp,x,x,x,x,x,x,verify:VR-LATER,abc,later
'''

VERIFY = '''record,ids,kind,references,revision,configurations,command,dirty,expected,outcome,limits,notes
VR-OLD,F01813,unit-test,test:old,abc,Debug;Release,cmd,clean,pass,pass,none,old
'''

GATES = '''slice,ids,old_bridges,ec,before,after,before_revision,evidence,notes
old-slice,F01813,symbol:x,EC-04,1,0,abc,verify:VR-OLD,old
'''

MODERNIZATION = '''id,symbol,final_name,path,track,status,issue,imp_step,lines,compile,body,copied,source_cpp,vs_namespace,typed_interface,no_bridge,strict_clean,behavior_evidence,generated_consumers,hand_consumers,test_consumers,module_table,legacy_calls,legacy_signature,source_flags,tests_execute,replay_execute,region_pct,verify_record,regression,notes
F01818,func_800E6178,Battle::Ai::CreateActorAiState,Native\\Game\\src\\Battle\\Ai\\ActorState.cpp,generated,todo,,,117,,,,,,,,,,,,,,,,,,,,,,
'''

PLAN = '''order,slice,status,owner,files,gate_slice,issue,commits,notes
28,battle-ai,active,self,Native\\Game\\src\\Battle\\Ai\\ActorState.cpp,old-slice,#276,aaa,F01818 remains generated; more
'''

SPEC = {
    "record": "VR-AI-ACTOR-CREATION", "slice": "battle-ai-actor-creation",
    "functions": {"F01818": {"catalog": {"proposed_system": "Game.Battle"}, "catalog_note": "; #276 F01818 typed",
                             "promotion_note": "{focused} cases"}},
    "verify": {"references": "test:creation", "command": "VS.Tests [ai-actor-creation]", "dirty": "clean",
               "expected": "pass", "limits": "unit only", "notes": "{focused} cases at {base}"},
    "gate": {"old_bridges": "symbol:func_800E6178", "ec": "EC-04", "before": "2", "after": "0", "notes": "gate"},
    "plan": {"order": "28", "commit": "bbb", "replace": [["F01818 remains generated", "F01818 typed"]]},
}


def write(path: Path, text: str, eol: str = "\n") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        stream.write(text.replace("\n", eol))


def run(repository: promote.Repository, *argv: str) -> str:
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        promote.main(list(argv), repository)
    return output.getvalue()


REGISTER = ("register", "F01818", "--typed", "VS::Battle::Ai::CreateActorAiState",
            "--source", r"Native\Game\src\Battle\Ai\ActorCreation.cpp",
            "--live", "VS::Battle::Ai::CreateLiveActorAiState", "--notes", "7CD68.c:934 func_800E6178, typed",
            "--since", "2026-02-03")


class PromoteTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="promote_test_"))
        self.addCleanup(shutil.rmtree, self.root, True)
        self.repository = promote.Repository(self.root)
        write(self.repository.fixups, FIXUPS, "\r\n")
        write(self.repository.solution, SOLUTION, "\r\n")
        write(self.repository.owned, OWNED, "\r\n")
        write(self.repository.ledger("FUNCTION_CATALOG"), CATALOG)
        write(self.repository.ledger("CPP_PROMOTION"), PROMOTION)
        write(self.repository.ledger("VERIFY_LEDGER"), VERIFY)
        write(self.repository.ledger("GATE_SLICES"), GATES)
        write(self.repository.ledger("MODERNIZATION_LEDGER"), MODERNIZATION)
        write(self.repository.ledger("PROMOTION_PLAN"), PLAN)

    def read(self, path: Path) -> str:
        with path.open(encoding="utf-8", newline="") as stream:
            return stream.read()

    def snapshot(self) -> dict[Path, bytes]:
        return {path: path.read_bytes() for path in self.root.rglob("*") if path.is_file() and path.name != "spec.json"}

    # ------------------------------------------------------------------ register
    def test_register_adds_every_entry_and_keeps_crlf(self) -> None:
        output = run(self.repository, *REGISTER)
        self.assertIn("NATIVE_HELPER_HEADERS: added", output)
        self.assertIn("FIXES: removed 8 lines", output)
        fixups = self.read(self.repository.fixups)
        self.assertNotIn("\n", fixups.replace("\r\n", ""))
        lines = fixups.split("\r\n")
        settings = lines.index('    "VS::Battle::Ai::ApplyLiveActorAiSettings": '
                               r'r"Native\Game\include\Battle\Ai\ActorSettingsLive.hpp",')
        self.assertEqual(lines[settings + 1], '    "VS::Battle::Ai::CreateLiveActorAiState": '
                                              r'r"Native\Game\include\Battle\Ai\ActorCreationLive.hpp",')
        binding = lines.index('OWNED_CALL_BINDINGS["F01813"] = "VS::Battle::Ai::ApplyLiveActorAiSettings"')
        self.assertEqual(lines[binding + 1], 'OWNED_CALL_BINDINGS["F01818"] = "VS::Battle::Ai::CreateLiveActorAiState"')
        self.assertIn('    ("BATTLE/BATTLE.PRG", 0x800E6178, "VS::Battle::Ai::CreateLiveActorAiState"),  # F01818',
                      lines)
        self.assertNotIn("F01818 keeps", fixups)
        self.assertIn("# The door fix stays.", fixups)
        self.assertIn('"F01900": []', fixups)
        ast.parse(fixups.replace("\r\n", "\n"))

        solution = self.read(self.repository.solution).split("\r\n")
        self.assertEqual(solution[solution.index(r'                r"src\Battle\Ai\ActorSettingsLive.cpp",') + 1],
                         r'                r"src\Battle\Ai\ActorCreationLive.cpp",')

        owned = self.read(self.repository.owned)
        self.assertTrue(owned.endswith('F01818,function,VS::Battle::Ai::CreateActorAiState,'
                                       r'Native\Game\src\Battle\Ai\ActorCreation.cpp,'
                                       r'Native\Game\include\Battle\Ai\ActorCreation.hpp,2026-02-03,'
                                       '"7CD68.c:934 func_800E6178, typed"\r\n'))
        self.assertIn("legacy, unquoted, notes\r\nF01818", owned)

    def test_register_twice_is_byte_identical(self) -> None:
        run(self.repository, *REGISTER)
        before = self.snapshot()
        output = run(self.repository, *REGISTER)
        self.assertEqual(before, self.snapshot())
        self.assertNotIn("added", output)
        self.assertIn("FIXES: absent", output)
        self.assertIn("owned.csv: unchanged", output)

    def test_register_keeps_an_existing_ownership_date(self) -> None:
        run(self.repository, *REGISTER)
        arguments = list(REGISTER)
        arguments[arguments.index("2026-02-03")] = "2027-01-01"
        run(self.repository, *arguments)
        self.assertIn("2026-02-03", self.read(self.repository.owned))
        self.assertNotIn("2027-01-01", self.read(self.repository.owned))

    def test_register_refuses_a_conflicting_binding(self) -> None:
        arguments = list(REGISTER)
        arguments[arguments.index("VS::Battle::Ai::CreateLiveActorAiState")] = "VS::Battle::Ai::Other"
        run(self.repository, *REGISTER)
        before = self.snapshot()
        with self.assertRaises(SystemExit):
            run(self.repository, *arguments)
        self.assertEqual(before, self.snapshot())

    def test_register_without_live_only_touches_fixes_and_owned(self) -> None:
        arguments = [argument for argument in REGISTER if argument not in
                     ("--live", "VS::Battle::Ai::CreateLiveActorAiState")]
        solution = self.read(self.repository.solution)
        output = run(self.repository, *arguments)
        self.assertNotIn("NATIVE_HELPER_HEADERS", output)
        self.assertEqual(solution, self.read(self.repository.solution))
        self.assertNotIn("F01818 keeps", self.read(self.repository.fixups))

    def test_csv_table_keeps_untouched_rows(self) -> None:
        table = promote.CsvTable(self.repository.owned)
        self.assertEqual(table.get("id", "F00920")["notes"], "legacy, unquoted, notes")
        table.upsert("id", {"id": "F00001", "kind": "function"}, "sorted")
        self.assertTrue(table.save())
        text = self.read(self.repository.owned)
        self.assertTrue(text.startswith("id,kind,name,path,header,since,notes\r\nF00001,function,,,,,\r\nF01813"))
        self.assertIn("legacy, unquoted, notes\r\n", text)

    # ------------------------------------------------------------------ adaptations
    def test_prune_hunks_drops_keeps_and_reanchors(self) -> None:
        rendered = {"a.cpp": "x\ninserted\none\ntwo\nthree\nfour", "b.cpp": "nothing"}
        table = {"a.cpp": [[1, ["one"], ["ONE"]], [3, ["three"], ["THREE"]], [4, ["missing"], []]],
                 "b.cpp": [[0, ["gone"], []]], "c.cpp": [[0, ["x"], []]]}
        kept, dropped = promote.prune_hunks(table, rendered)
        self.assertEqual(kept, {"a.cpp": [[2, ["one"], ["ONE"]], [4, ["three"], ["THREE"]]]})
        self.assertEqual(len(dropped), 3)
        self.assertTrue(any(line.startswith("c.cpp: whole file") for line in dropped))

    def test_write_adaptations_matches_the_recorder_format(self) -> None:
        path = self.root / "adaptations.json"
        table = {"b.cpp": [[0, ["x"], ["y"]]], "a.cpp": []}
        path.write_bytes(b"{}\n")
        self.assertTrue(promote.write_adaptations(path, table))
        self.assertEqual(path.read_text(encoding="utf-8"), json.dumps(table, indent=0, sort_keys=True) + "\n")
        self.assertFalse(promote.write_adaptations(path, table))

    # ------------------------------------------------------------------ ledgers
    def ledgers(self, spec: dict, *extra: str) -> str:
        path = self.root / "spec.json"
        path.write_text(json.dumps(spec), encoding="utf-8")
        return run(self.repository, "ledgers", str(path), "--no-sync", "--set", "base=0123abc", *extra)

    def test_ledgers_fill_every_table_and_are_idempotent(self) -> None:
        run(self.repository, *REGISTER)
        self.ledgers(SPEC, "--set", "focused=12")
        before = self.snapshot()
        self.assertIn("unchanged", self.ledgers(SPEC, "--set", "focused=12"))
        self.assertEqual(before, self.snapshot())

        catalog = promote.CsvTable(self.repository.ledger("FUNCTION_CATALOG")).get("id", "F01818")
        self.assertEqual(catalog["final_location"], r"Native\Game\src\Battle\Ai\ActorCreation.cpp")
        self.assertEqual((catalog["named"], catalog["verified"], catalog["proposed_system"]), ("x", "x", "Game.Battle"))
        self.assertEqual(catalog["notes"], "conf:high; #276 F01818 typed")

        promotion = promote.CsvTable(self.repository.ledger("CPP_PROMOTION"))
        self.assertEqual([row["id"] for row, _ in promotion.rows], ["F01813", "F01818", "F01900"])
        self.assertEqual(promotion.get("id", "F01818")["notes"], "12 cases")
        self.assertEqual(promotion.get("id", "F01818")["evidence"], "verify:VR-AI-ACTOR-CREATION")

        verify = promote.CsvTable(self.repository.ledger("VERIFY_LEDGER"))
        self.assertEqual(verify.rows[0][0]["record"], "VR-AI-ACTOR-CREATION")
        self.assertEqual(verify.rows[0][0]["notes"], "12 cases at 0123abc")
        self.assertEqual(promote.CsvTable(self.repository.ledger("GATE_SLICES")).rows[0][0]["slice"],
                         "battle-ai-actor-creation")
        self.assertEqual(promote.CsvTable(self.repository.ledger("MODERNIZATION_LEDGER")).get("id", "F01818")["status"],
                         "done")
        plan = promote.CsvTable(self.repository.ledger("PROMOTION_PLAN")).get("order", "28")
        self.assertEqual((plan["gate_slice"], plan["commits"]), ("old-slice;battle-ai-actor-creation", "aaa;bbb"))
        self.assertEqual(plan["notes"], "F01818 typed; more")

    def test_ledgers_reject_unfilled_placeholders(self) -> None:
        run(self.repository, *REGISTER)
        before = self.snapshot()
        with self.assertRaises(SystemExit) as raised:
            self.ledgers(SPEC)
        self.assertIn("focused", str(raised.exception))
        self.assertEqual(before, self.snapshot())

    def test_ledgers_need_an_owned_row(self) -> None:
        with self.assertRaises(SystemExit):
            self.ledgers(SPEC, "--set", "focused=1")

    def test_ledgers_need_the_verify_fields(self) -> None:
        run(self.repository, *REGISTER)
        spec = json.loads(json.dumps(SPEC))
        del spec["verify"]["limits"]
        with self.assertRaises(SystemExit):
            self.ledgers(spec, "--set", "focused=1")

    def test_ledgers_reject_a_missing_plan_note(self) -> None:
        run(self.repository, *REGISTER)
        spec = json.loads(json.dumps(SPEC))
        spec["plan"]["replace"] = [["not in the notes", "nor this"]]
        with self.assertRaises(SystemExit):
            self.ledgers(spec, "--set", "focused=1")

    def test_progress_keeps_the_original_cohort_and_distinguishes_retirement(self) -> None:
        fields = ("id,native_name,native_file,plan_order,plan_status,track,modernization_status,cpp_status,"
                  "copied_to_native,compile_mode,behavior_verified,tests_execute,replay_execute,snapshot,purpose")
        path = self.repository.ledger("REMAINING_FUNCTIONS")
        write(path, fields + "\n" +
              "\n".join(f"{identifier},old,old.cpp,28,active,generated,todo,not started,"
                        f"yes,relaxed,no,no,yes,original-sha,\"purpose, {identifier}\""
                        for identifier in ("F01818", "F01819", "F01814")) + "\n", "\r\n")
        catalog = promote.CsvTable(self.repository.ledger("FUNCTION_CATALOG"))
        catalog.update("id", "F01818", lambda row: row.update(
            {"cpp": "x", "final_location": "new.cpp", "verified": "x"}))
        catalog.update("id", "F01819", lambda row: row.update({"cpp": "wip"}))
        catalog.update("id", "F01814", lambda row: row.update({"copied": "retired", "copy": "no"}))
        catalog.save()
        modern = promote.CsvTable(self.repository.ledger("MODERNIZATION_LEDGER"))
        modern.update("id", "F01818", lambda row: row.update(
            {"track": "owned", "status": "done", "compile": "strict", "tests_execute": "yes",
             "replay_execute": "no"}))
        modern.upsert("id", {"id": "F01819", "track": "owned", "status": "todo", "compile": "strict"}, "sorted")
        modern.save()
        self.assertEqual(promote.sync_remaining_functions(self.repository), 0)
        first = path.read_bytes()
        self.assertEqual(promote.sync_remaining_functions(self.repository), 0)
        self.assertEqual(path.read_bytes(), first)
        rows = [row for row, _ in promote.CsvTable(path).rows]
        self.assertEqual([row["id"] for row in rows], ["F01818", "F01819", "F01814"])
        self.assertEqual([row["cpp_status"] for row in rows], ["promoted", "in progress", "retired"])
        self.assertEqual([row["snapshot"] for row in rows], ["original-sha"] * 3)
        self.assertEqual((rows[0]["native_file"], rows[0]["behavior_verified"], rows[0]["replay_execute"]),
                         ("new.cpp", "yes", "no"))
        self.assertEqual(rows[2]["compile_mode"], "not applicable")
        self.assertNotIn(b"\n", first.replace(b"\r\n", b""))

    def test_progress_missing_canonical_row_does_not_write_partial_results(self) -> None:
        path = self.repository.ledger("REMAINING_FUNCTIONS")
        write(path, "id,native_name,native_file,plan_order,plan_status,track,modernization_status,cpp_status,"
              "copied_to_native,compile_mode,behavior_verified,tests_execute,replay_execute\n"
              "F01818,old,old.cpp,28,active,generated,todo,not started,yes,relaxed,no,no,yes\n"
              "F99999,missing,missing.cpp,28,active,generated,todo,not started,yes,relaxed,no,no,yes\n")
        before = path.read_bytes()
        with self.assertRaises(SystemExit):
            promote.sync_remaining_functions(self.repository)
        self.assertEqual(path.read_bytes(), before)

    def test_failed_ledger_check_does_not_refresh_progress(self) -> None:
        with patch.object(promote.subprocess, "run") as command, \
                patch.object(promote, "sync_remaining_functions") as refresh:
            command.return_value.returncode = 1
            command.return_value.stdout = "failed"
            command.return_value.stderr = ""
            self.assertEqual(promote.sync_ledgers(self.repository), 1)
            refresh.assert_not_called()

    # ------------------------------------------------------------------ scaffold and show
    def test_scaffold_writes_templates_once(self) -> None:
        arguments = ("scaffold", "F01818", "--case", "actor_creation", "--typed", "VS::Battle::Ai::CreateActorAiState",
                     "--source", r"Native\Game\src\Battle\Ai\ActorCreation.cpp",
                     "--test", r"Native\Tests\src\AiActorCreationTests.cpp", "--tag", "ai-actor-creation")
        output = run(self.repository, *arguments)
        self.assertEqual(output.count("wrote"), 3)
        case = (self.repository.cases / "actor_creation.py").read_text(encoding="utf-8")
        ast.parse(case)
        self.assertIn("ENTRY, END = 0x800E6178, 0x800E65DC", case)
        driver = (self.repository.cases / "actor_creation_driver.cpp").read_text(encoding="utf-8")
        self.assertIn('#include "Battle/Ai/ActorCreation.hpp"', driver)
        self.assertIn(r"tools\port\differentials\actor_creation.py", driver)
        self.assertIn('std::printf("\\n");', driver)
        test = (self.root / r"Native\Tests\src\AiActorCreationTests.cpp").read_text(encoding="utf-8")
        self.assertIn('"[ai-actor-creation]"', test)

        (self.repository.cases / "actor_creation.py").write_text("edited", encoding="utf-8")
        output = run(self.repository, *arguments)
        self.assertEqual(output.count("exists, left alone"), 3)
        self.assertEqual((self.repository.cases / "actor_creation.py").read_text(encoding="utf-8"), "edited")

    def test_show_reports_the_address_range(self) -> None:
        output = run(self.repository, "show", "F01818")
        self.assertIn("module BATTLE/BATTLE.PRG", output)
        self.assertIn("range 0x800E6178-0x800E65DC (281 words)", output)
        self.assertIn("not owned", output)
        self.assertIn("range 0x800E5A9C-0x800E5EC0", run(self.repository, "show", "F01813"))


if __name__ == "__main__":
    unittest.main()
