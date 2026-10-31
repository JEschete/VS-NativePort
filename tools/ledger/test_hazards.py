"""Synthetic controls for the platform-assumption hazard ledger; no disc data or live ledgers."""

from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hazards  # noqa: E402

CATALOG_FIELDS = ["id", "name", "copy", "final_name", "final_location", "copied", "cpp", "notes"]
OWNED_FIELDS = ["id", "kind", "name", "path", "header", "since", "notes"]
SOURCE = "Native/Game/src/Battle/Sample.cpp"
HEADER = "Native/Game/src/Battle/Sample.h"


def scan_all(body: str, path: str = SOURCE) -> list[hazards.Hazard]:
    with tempfile.TemporaryDirectory() as directory:
        fixture = Fixture(Path(directory))
        fixture.files[path] = body
        return hazards.scan(fixture.write())


def findings(body: str, path: str = SOURCE) -> list[hazards.Hazard]:
    """Function-level sites; the fixture's own table declaration is covered by CapacityTests."""
    return [item for item in scan_all(body, path) if item.hazard_class != "CAPACITY"]


def classes(body: str, path: str = SOURCE) -> set[str]:
    return {item.hazard_class for item in findings(body, path)}


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.files = {
            HEADER: "extern Actor* Actors_actorRecords[17];\nextern Thing* Thing_current;\n",
        }
        self.catalog = [
            {"id": "F00001", "name": "func_1", "copy": "yes", "final_name": "Battle::Hit",
             "final_location": SOURCE.replace("/", "\\"), "copied": "x", "cpp": "", "notes": ""},
        ]

    def write(self) -> hazards.HazardWorkspace:
        ledgers = self.root / "Docs" / "ledgers"
        ledgers.mkdir(parents=True, exist_ok=True)
        with (ledgers / "FUNCTION_CATALOG.csv").open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, CATALOG_FIELDS)
            writer.writeheader()
            writer.writerows(self.catalog)
        port = self.root / "tools" / "port"
        port.mkdir(parents=True, exist_ok=True)
        with (port / "owned.csv").open("w", encoding="utf-8", newline="") as stream:
            csv.DictWriter(stream, OWNED_FIELDS).writeheader()
        (port / "copy_manifest.json").write_text(json.dumps({}), encoding="utf-8")
        for path, text in self.files.items():
            full = self.root / path
            full.parent.mkdir(parents=True, exist_ok=True)
            full.write_text(text, encoding="utf-8")
        tracker = self.root / "Docs" / "PLATFORM_HAZARDS.md"
        if not tracker.exists():
            tracker.write_text(f"# Hazards\n\n{hazards.SUMMARY_BEGIN}\n{hazards.SUMMARY_END}\n", encoding="utf-8")
        return hazards.HazardWorkspace(self.root)


class NullTests(unittest.TestCase):
    def test_the_251_shape_is_found(self) -> None:
        # The real F00818 reads through a record that gimmick actors never have.
        found = findings("int Battle_Hit(int id) { return Actors_actorRecords[id]->flags; }\n")
        self.assertEqual([item.hazard_class for item in found], ["NULL-TABLE"])
        self.assertEqual(found[0].function, "F00001")
        self.assertEqual(found[0].line, 1)

    def test_a_cast_table_read_is_found(self) -> None:
        self.assertIn("NULL-TABLE", classes("int Battle_Hit(int id) { return ((View*)Actors_actorRecords[id])->x; }\n"))

    def test_a_local_alias_of_a_table_entry_is_found(self) -> None:
        body = "int Battle_Hit(int id) { Actor* a = Actors_actorRecords[id]; return a->flags; }\n"
        self.assertIn("NULL-TABLE", classes(body))

    def test_a_guard_before_the_read_clears_it(self) -> None:
        for guard in ("Actors_actorRecords[id] != NULL", "Actors_actorRecords[id] != nullptr", "Actors_actorRecords[id]"):
            body = f"int Battle_Hit(int id) {{ if ({guard}) return Actors_actorRecords[id]->flags; return 0; }}\n"
            self.assertNotIn("NULL-TABLE", classes(body), guard)

    def test_an_alias_guard_clears_it(self) -> None:
        body = "int Battle_Hit(int id) { Actor* a = Actors_actorRecords[id]; if (!a) return 0; return a->flags; }\n"
        self.assertEqual(classes(body), set())

    def test_a_constant_index_is_marked_lower_confidence(self) -> None:
        found = findings("int Battle_Hit(void) { return Actors_actorRecords[0]->flags; }\n")
        self.assertEqual(found[0].confidence, "low")
        self.assertEqual(findings("int Battle_Hit(int i) { return Actors_actorRecords[i]->flags; }\n")[0].confidence, "medium")

    def test_a_guard_after_the_read_is_its_own_class(self) -> None:
        # Release removes this guard, so the source looks safer than the program is.
        body = "int Battle_Hit(Actor* a) { int v = a->flags; if (a == NULL) return 0; return v; }\n"
        self.assertIn("GUARD-AFTER-READ", classes(body))

    def test_a_guard_before_the_read_is_not_after_read(self) -> None:
        self.assertEqual(classes("int Battle_Hit(Actor* a) { if (a == NULL) return 0; return a->flags; }\n"), set())

    def test_testing_a_field_is_not_a_guard_on_the_pointer(self) -> None:
        # The real F00818 reads `!actor->instance.motionLocked`, which proves nothing about `actor`.
        body = ("int Battle_Hit(int id) { Actor* actor = Actors_actorRecords[id]; "
                "if (!actor->locked) return 1; return actor->flags; }\n")
        self.assertEqual(classes(body), {"NULL-TABLE"})

    def test_testing_a_table_entrys_field_is_not_a_guard(self) -> None:
        body = "int Battle_Hit(int id) { if (!Actors_actorRecords[id]->locked) return 1; return 0; }\n"
        self.assertEqual(classes(body), {"NULL-TABLE"})

    def test_reading_a_field_twice_is_not_a_guard_after_read(self) -> None:
        self.assertEqual(classes("int Battle_Hit(Actor* a) { int v = a->x; if (!a->y) return 0; return v; }\n"), set())

    def test_the_real_camera_shape_is_a_guard_after_read(self) -> None:
        # F00118 reads Field_cameraTransition several times and only then compares it with NULL.
        body = ("void Battle_Hit(void) { use(Field_cameraTransition->from); "
                "if (Field_cameraTransition != NULL) release(Field_cameraTransition); }\n")
        self.assertEqual(classes(body), {"GUARD-AFTER-READ"})

    def test_sizeof_does_not_read_its_operand(self) -> None:
        body = "int Battle_Hit(void) { int n = sizeof state->queue; if (state == nullptr) abort(); return state->x + n; }\n"
        self.assertEqual(classes(body), set())

    def test_a_loop_condition_guards_its_body(self) -> None:
        body = "int Battle_Hit(Actor* a) { for (a = head; a; a = a->next) use(a->x); if (!a) return 1; return 0; }\n"
        self.assertEqual(classes(body), set())

    def test_a_lookup_result_read_directly_is_found(self) -> None:
        self.assertIn("NULL-RETURN", classes("int Battle_Hit(int i) { return Field_FindThing(i)->flags; }\n"))

    def test_a_lookup_result_kept_and_guarded_is_clean(self) -> None:
        body = "int Battle_Hit(int i) { Thing* t = Field_FindThing(i); if (t) return t->flags; return 0; }\n"
        self.assertEqual(classes(body), set())

    def test_a_lookup_result_kept_and_read_unguarded_is_found(self) -> None:
        self.assertIn("NULL-RETURN", classes("int Battle_Hit(int i) { Thing* t = Field_FindThing(i); return t->flags; }\n"))


class DivisionTests(unittest.TestCase):
    def test_a_variable_divisor_is_found(self) -> None:
        self.assertEqual(classes("int Battle_Hit(int a, int b) { return a / b; }\n"), {"DIV-ZERO"})
        self.assertEqual(classes("int Battle_Hit(int a, int b) { return a % b; }\n"), {"DIV-ZERO"})

    def test_a_member_divisor_is_found(self) -> None:
        self.assertIn("DIV-ZERO", classes("int Battle_Hit(int a, T* t) { if (t) return a / t->count; return 0; }\n"))

    def test_constant_divisors_are_clean(self) -> None:
        for divisor in ("4", "0x10", "kUnits", "MAX_UNITS", "sizeof(int)"):
            self.assertEqual(classes(f"int Battle_Hit(int a) {{ return a / {divisor}; }}\n"), set(), divisor)

    def test_a_guarded_divisor_is_clean(self) -> None:
        self.assertEqual(classes("int Battle_Hit(int a, int b) { if (b != 0) return a / b; return 0; }\n"), set())
        self.assertEqual(classes("int Battle_Hit(int a, int b) { if (!b) return 0; return a / b; }\n"), set())

    def test_joining_filesystem_paths_is_not_division(self) -> None:
        self.assertEqual(classes("auto Battle_Hit(const fs::path& root, const fs::path& name) { return root / name; }\n"), set())

    def test_floating_point_is_clean(self) -> None:
        self.assertEqual(classes("float Battle_Hit(float a, float b) { return a / b; }\n"), set())

    def test_a_comment_or_string_is_not_a_division(self) -> None:
        self.assertEqual(classes('int Battle_Hit(int a) { /* a / b */ const char* s = "c / d"; return a; }\n'), set())


class LayoutTests(unittest.TestCase):
    def test_a_literal_size_copy_is_found(self) -> None:
        self.assertEqual(classes("void Battle_Hit(T* a) { memset(a, 0, 0x178); }\n"), {"LITERAL-SIZE"})

    def test_a_sizeof_copy_is_clean(self) -> None:
        self.assertEqual(classes("void Battle_Hit(T* a) { memset(a, 0, sizeof(*a)); }\n"), set())

    def test_a_byte_offset_read_is_found(self) -> None:
        self.assertEqual(classes("int Battle_Hit(T* a) { return *(int*)((char*)a + 0x18C); }\n"), {"BYTE-OFFSET"})

    def test_an_address_offset_read_is_found(self) -> None:
        self.assertEqual(classes("int Battle_Hit(T* a) { return *(int*)((uintptr_t)&a->b + 8); }\n"), {"BYTE-OFFSET"})

    def test_a_ps1_address_literal_is_found(self) -> None:
        self.assertEqual(classes("int* Battle_Hit(void) { return (int*)0x80100000; }\n"), {"PS1-ADDRESS"})
        self.assertEqual(classes("int* Battle_Hit(void) { return (int*)0x1F800000; }\n"), {"PS1-ADDRESS"})

    def test_the_address_table_itself_is_not_a_hazard_site(self) -> None:
        body = 'void* table[] = {"M", (void*)0x80100000};\n'
        self.assertEqual(classes(body, "Native/Game/src/Boot/ModulePointers.cpp"), set())

    def test_an_unprototyped_call_target_is_found(self) -> None:
        self.assertEqual(classes("int Battle_Hit(int (*f)(...)) { return f(1, 2); }\n"), {"UNPROTOTYPED-CALL"})

    def test_an_unsized_module_array_is_found(self) -> None:
        self.assertEqual(classes("short Effects_rows[][4] = {{1, 2, 3, 4}};\n"), {"UNSIZED-ARRAY"})


class CapacityTests(unittest.TestCase):
    INVENTORY = "Native/Game/include/Items/Inventory.h"

    def test_a_fixed_inventory_array_is_a_capacity_row(self) -> None:
        header = "typedef struct Items_Inventory {\n    Items_InventoryGem gems[48];\n} Items_Inventory;\n"
        found = [item.detail for item in scan_all(header, self.INVENTORY) if item.hazard_class == "CAPACITY"]
        self.assertIn("Items_Inventory.gems[48]", found)

    def test_a_capacity_row_points_at_its_field_not_an_earlier_mention(self) -> None:
        header = "typedef struct Items_Inventory Items_Inventory;\nint gems;\n\ntypedef struct Items_Inventory {\n    Items_InventoryGem gems[48];\n} Items_Inventory;\n"
        found = [item for item in scan_all(header, self.INVENTORY) if item.detail == "Items_Inventory.gems[48]"]
        self.assertEqual(found[0].line, 5)

    def test_a_narrow_id_field_is_found(self) -> None:
        header = "typedef struct Items_InventoryWeapon {\n    u_char blade;\n    char isEquipped;\n} Items_InventoryWeapon;\n"
        found = [item.detail for item in scan_all(header, self.INVENTORY) if item.hazard_class == "NARROW-ID"]
        self.assertEqual(found, ["Items_InventoryWeapon.blade"])

    def test_a_hard_coded_capacity_loop_over_inventory_is_found(self) -> None:
        body = "void Battle_Hit(Items_Inventory* inv) { for (i = 0; i < 48; ++i) inv->gems[i].id = 0; }\n"
        self.assertEqual(classes(body), {"CAPACITY-LITERAL"})

    def test_a_literal_loop_elsewhere_is_clean(self) -> None:
        self.assertEqual(classes("void Battle_Hit(T* t) { for (i = 0; i < 48; ++i) t->rows[i] = 0; }\n"), set())

    def test_a_fixed_pointer_table_is_a_capacity_row(self) -> None:
        found = [item.detail for item in scan_all("int x;\n") if item.hazard_class == "CAPACITY"]
        self.assertEqual(found, ["Actors_actorRecords[17]"])


class ModuleDataTests(unittest.TestCase):
    def test_the_copy_tools_placeholder_row_is_found(self) -> None:
        # What copy_to_native.py emits when it has no size: one row, no initializer, and no bytes from the module image.
        self.assertEqual(classes("u_char Effects_Plg049_ringColorFactors[1][4]; /* contents: module image */\n"),
                         {"UNSIZED-ARRAY"})

    def test_a_real_one_element_initialized_array_is_clean(self) -> None:
        self.assertEqual(classes("int Effects_rows[1] = {4};\n"), set())

    def test_a_flag_mask_below_program_memory_is_not_an_address(self) -> None:
        for mask in ("0x80000000", "0x80000014", "0x80008000"):
            self.assertEqual(classes(f"int Battle_Hit(int a) {{ return a & {mask}; }}\n"), set(), mask)

    def test_an_address_shaped_literal_is_low_confidence(self) -> None:
        # 0x80040000 can be a real RAM address or a set of flags; only reading it can tell.
        self.assertEqual(findings("int Battle_Hit(int a) { return a & 0x80040000; }\n")[0].confidence, "low")


class CompilerLogTests(unittest.TestCase):
    LOG = (
        r"D:\Repo\Native\Game\src\Battle\Sample.cpp(1,31): warning : non-void function does not return a value "
        r"[-Wreturn-type] [D:\Repo\Native\Game\VS.Game.vcxproj]"
        "\n"
        r"D:\Repo\Native\Game\src\Battle\Sample.cpp(1,31): warning : non-void function does not return a value "
        r"[-Wreturn-type] [D:\Repo\Native\Game\VS.Game.vcxproj]"
        "\n"
        r"D:\Repo\Native\Game\src\Battle\Sample.cpp(2,5): warning : variable 'x' is uninitialized [-Wuninitialized]"
        "\n"
        r"D:\Repo\Native\Game\src\Battle\Sample.cpp(3,5): warning : macro redefined [-Wmacro-redefined]"
        "\n"
        r"D:\Repo\vcpkg_installed\include\x.h(3,5): warning : cast [-Wpointer-to-int-cast]"
        "\n"
    )

    def test_a_project_relative_path_resolves_through_its_project(self) -> None:
        # MSBuild's real form: the path is relative to the project named at the end of the line.
        log = (r"src\Battle\Sample.cpp(5,1): warning : non-void function does not return a value in all control paths "
               r"[-Wreturn-type] [D:\Repo\Native\Game\VS.Game.vcxproj]")
        found = hazards.parse_compiler_log(log, r"D:\Repo")
        self.assertEqual([(item.hazard_class, item.path, item.line) for item in found],
                         [("RETURN-FALLOFF", SOURCE, 5)])

    def test_a_parent_relative_path_resolves_through_its_project(self) -> None:
        log = (r"..\Engine\src\Core\Sample.cpp(5,1): warning : x [-Wuninitialized] "
               r"[D:\Repo\Native\Game\VS.Game.vcxproj]")
        self.assertEqual(hazards.parse_compiler_log(log, r"D:\Repo")[0].path, "Native/Engine/src/Core/Sample.cpp")

    def parsed(self) -> list[tuple[str, str, int]]:
        return [(item.hazard_class, item.path, item.line) for item in hazards.parse_compiler_log(self.LOG, r"D:\Repo")]

    def test_relevant_warnings_become_rows_once(self) -> None:
        self.assertEqual(self.parsed(), [
            ("RETURN-FALLOFF", SOURCE, 1),
            ("UNINITIALIZED", SOURCE, 2),
        ])

    def test_compiler_rows_are_high_confidence(self) -> None:
        self.assertEqual({item.confidence for item in hazards.parse_compiler_log(self.LOG, r"D:\Repo")}, {"high"})


class LedgerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.fixture = Fixture(Path(self.directory.name))
        self.fixture.files[SOURCE] = "int Battle_Hit(int id) { return Actors_actorRecords[id]->flags; }\n"
        self.workspace = self.fixture.write()

    def rows(self) -> list[dict[str, str]]:
        return hazards.read_csv(self.workspace.ledger)

    def rules(self) -> set[str]:
        return {issue.rule for issue in hazards.check(self.workspace)}

    def source(self, text: str) -> None:
        self.fixture.files[SOURCE] = text
        self.fixture.write()

    def set_row(self, **values: str) -> None:
        rows = self.rows()
        rows[0].update(values)
        hazards.write_ledger(self.workspace, rows)
        hazards.write_tracker(self.workspace, rows)

    def test_sync_then_check_is_clean(self) -> None:
        hazards.sync(self.workspace)
        self.assertEqual(self.rules(), set())

    def test_a_missing_ledger_is_reported(self) -> None:
        self.assertEqual(self.rules(), {"ledger-missing"})

    def test_a_new_site_fails_until_acknowledged(self) -> None:
        hazards.sync(self.workspace)
        self.source(self.fixture.files[SOURCE] + "int Battle_Other(int a, int b) { return a / b; }\n")
        self.assertIn("hazard-new", self.rules())
        hazards.sync(self.workspace)
        self.assertEqual(self.rules(), set())

    def test_moving_code_does_not_change_its_key(self) -> None:
        hazards.sync(self.workspace)
        key = {row["key"] for row in self.rows()}
        self.source("\n\n\n" + self.fixture.files[SOURCE])
        self.assertEqual(self.rules(), set())
        hazards.sync(self.workspace)
        self.assertEqual({row["key"] for row in self.rows()}, key)
        self.assertEqual([row["line"] for row in self.rows() if row["class"] == "NULL-TABLE"], ["4"])

    def test_a_fixed_site_leaves_the_ledger_on_sync(self) -> None:
        hazards.sync(self.workspace)
        self.source("int Battle_Hit(int id) { if (Actors_actorRecords[id]) return 1; return 0; }\n")
        self.assertIn("hazard-gone", self.rules())
        hazards.sync(self.workspace)
        self.assertEqual([row for row in self.rows() if row["class"] == "NULL-TABLE"], [])

    def test_triage_survives_sync(self) -> None:
        hazards.sync(self.workspace)
        self.set_row(status="false-positive", notes="always loaded here")
        hazards.sync(self.workspace)
        self.assertEqual((self.rows()[0]["status"], self.rows()[0]["notes"]), ("false-positive", "always loaded here"))

    def test_an_unknown_status_is_rejected(self) -> None:
        hazards.sync(self.workspace)
        self.set_row(status="whatever")
        self.assertIn("status-unknown", self.rules())

    def test_closing_a_row_without_a_reason_is_rejected(self) -> None:
        hazards.sync(self.workspace)
        self.set_row(status="false-positive")
        self.assertIn("triage-without-reason", self.rules())

    def test_compiler_rows_survive_a_scan_only_sync(self) -> None:
        log = (rf"{self.fixture.root}\Native\Game\src\Battle\Sample.cpp(1,1): warning : uninitialized "
               r"[-Wuninitialized]")
        hazards.sync(self.workspace, log)
        self.assertIn("UNINITIALIZED", {row["class"] for row in self.rows()})
        hazards.sync(self.workspace)
        self.assertIn("UNINITIALIZED", {row["class"] for row in self.rows()})

    def test_a_compiler_row_names_the_function_it_is_in(self) -> None:
        self.source("int Battle_Other(void) {\n  return 1;\n}\nint Battle_Hit(int id) {\n  int x;\n  return x;\n}\n")
        log = rf"{self.fixture.root}\Native\Game\src\Battle\Sample.cpp(6,10): warning : uninitialized [-Wuninitialized]"
        hazards.sync(self.workspace, log)
        row = next(row for row in self.rows() if row["class"] == "UNINITIALIZED")
        self.assertEqual((row["symbol"], row["function"]), ("Battle_Hit", "F00001"))

    def test_the_tracker_block_must_match_the_ledger(self) -> None:
        hazards.sync(self.workspace)
        rows = self.rows()
        rows[0]["class"] = "DIV-ZERO"
        hazards.write_ledger(self.workspace, rows)
        self.assertIn("tracker-stale", self.rules())

    def test_every_class_documents_its_replacement(self) -> None:
        for name, entry in hazards.CLASSES.items():
            self.assertTrue(entry.ps1 and entry.native and entry.modern, name)


if __name__ == "__main__":
    unittest.main()
