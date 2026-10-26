"""Checks for the read-only, provenance-preserving assembly audit."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.port import audit_assembly_gaps as audit_module
from tools.port.audit_assembly_gaps import (
    audit,
    canonical_id,
    counterpart_names,
    definitions,
    native_status,
    overlay,
    plain_source,
    redirect_status,
    source_index,
    template_status,
)


class AssemblyGapAuditTests(unittest.TestCase):
    def test_published_report_does_not_claim_an_unversioned_pin_is_a_parent_head(self):
        reference = {
            "revision": "unversioned snapshot",
            "source_files_scanned": 1,
            "source_fingerprint_sha256": "snapshot-source-fingerprint",
        }
        summary = {
            "catalog_rows": 0,
            "port_target_rows": 0,
            "gap_source_counts": {},
            "source_label_coverage": {
                "uncataloged_game_labels": 0,
                "uncataloged_sdk_labels": 0,
            },
            "references": {"rood-reverseWin": reference},
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            docs = root / "Docs"
            docs.mkdir()
            with patch.object(audit_module, "REPO", root):
                audit_module.publish_report(summary, [], [])
            report = (docs / "ASSEMBLY_GAPS.md").read_text(encoding="utf-8")
            self.assertIn(
                "| `rood-reverseWin` | `unversioned snapshot` | 1 | "
                "`snapshot-source-fingerprint` |",
                report,
            )
            self.assertNotIn("listed revision is the parent revision", report)
            self.assertIn(
                "A Git revision is recorded only for a reference repository whose",
                report,
            )

    def test_reference_revision_does_not_attribute_an_enclosing_repository(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "snapshot"
            root.mkdir()
            with patch.object(audit_module, "git_output") as output:
                self.assertEqual(
                    audit_module.reference_revision(root, directory),
                    "unversioned snapshot",
                )
                output.assert_not_called()

    def test_reference_revision_records_the_reference_repository_head(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(
                audit_module, "git_output", return_value="reference-head"
            ) as output:
                self.assertEqual(
                    audit_module.reference_revision(root, str(root)),
                    "reference-head",
                )
                output.assert_called_once_with(root, "rev-parse", "HEAD")

    def test_redirect_requires_the_expected_macro_and_a_real_native_body(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = r"Native\Engine\src\Core\Gte.cpp"
            header = root / r"Native\Engine\src\Core\Math\Probe.h"
            body_file = root / source
            header.parent.mkdir(parents=True)
            body_file.parent.mkdir(parents=True, exist_ok=True)
            row = {"id": "Fprobe", "final_name": "Core::Probe",
                   "final_location": r"Native\Engine\src\Core\Math\Probe.c"}
            real = "int Native_Target(int value) { return value + 1; }"
            cases = [
                ("#define Core_Probe Native_Target", real, "alias-implemented"),
                ("#define Core_Probe Native_Other", real, "missing-definition"),
                ("/* #define Core_Probe Native_Target */", real, "missing-definition"),
                ("#define Core_Probe Native_Target", "int Native_Target(int);",
                 "missing-definition"),
                ("#define Core_Probe Native_Target",
                 'int Native_Target(int value) { VS_UNPORTED("gap"); return 0; }', "stub"),
            ]
            with (patch.object(audit_module, "REPO", root),
                  patch.dict(audit_module.NATIVE_FUNCTION_REDIRECTS,
                             {"Fprobe": ("Native_Target", source)})):
                for macro, body, expected in cases:
                    with self.subTest(macro=macro, body=body):
                        header.write_text(macro, encoding="utf-8")
                        body_file.write_text(body, encoding="utf-8")
                        self.assertEqual(redirect_status(row)[0], expected)

    def test_calls_comments_literals_and_prototypes_are_not_definitions(self):
        text = '''
// int target(void) { return 0; }
const char* example = "target() { }";
int target(int value);
int caller(void) { return target(3); }
int target(int value) {
    if (value) { return value; }
    return 0;
}
'''
        found = definitions(text, {"target"})
        self.assertEqual(len(found), 1)
        self.assertIn("return value;", found[0].body)
        self.assertEqual(found[0].line, 6)

    def test_cpp_noexcept_raw_literals_and_nested_arguments(self):
        text = '''
const auto example = R"code(target() { " } // })code";
extern "C" int target(int (*callback)(int)) noexcept {
    return callback(1);
}
'''
        found = definitions(text, {"target"})
        self.assertEqual(len(found), 1)
        self.assertIn("callback(1)", found[0].body)

    def test_native_stub_and_partial_are_distinguished(self):
        self.assertEqual(native_status('VS_UNPORTED("gap"); return 0;'), "stub")
        self.assertEqual(native_status('VS_UNPORTED("gap");'), "stub")
        self.assertEqual(native_status('return 0; // VS_UNPORTED("not code")'), "implemented")
        self.assertEqual(native_status('if (x) VS_UNPORTED("gap"); return x;'),
                         "partial-unported")

    def test_zero_record_stub_is_not_counted_as_implementation(self):
        self.assertEqual(native_status('VS_UNPORTED("gap"); Record result = {0}; return result;'),
                         "stub")

    def test_inline_assembly_is_not_plain_c(self):
        self.assertFalse(plain_source('__asm__ volatile("nop");'))
        self.assertTrue(plain_source('return 0; /* __asm__ */'))

    def test_psyq_assembly_function_prefix_is_not_plain_c(self):
        found = definitions("asm void target(register int a0) { jr ra; nop; }", {"target"})
        self.assertEqual(len(found), 1)
        self.assertTrue(found[0].assembly)

    def test_duplicate_chain_resolves_and_cycles_fail_explicitly(self):
        catalog = {"F1": {"id": "F1", "notes": "asm; dup-of:F2"},
                   "F2": {"id": "F2", "notes": "dup-of:F3"},
                   "F3": {"id": "F3", "notes": ""}}
        self.assertEqual(canonical_id(catalog["F1"], catalog), "F3")
        catalog["F3"]["notes"] = "dup-of:F1"
        with self.assertRaises(ValueError):
            canonical_id(catalog["F1"], catalog)

    def test_overlay_scope_includes_resident_and_prg_modules(self):
        self.assertEqual(overlay(Path(r"src\SLUS_010.40\main.c")), "SLUS_010.40")
        self.assertEqual(overlay(Path(r"src\BATTLE\BATTLE.PRG\unit.c")), "BATTLE.PRG")
        self.assertIsNone(overlay(Path(r"pc\src\bridge.cpp")))

    def test_renamed_address_lookup_never_crosses_overlays(self):
        row = {"name": "func_800F9800", "overlay": "MENU1.PRG"}
        symbols = {("MENU1.PRG", "Renamed"): 0x800F9800,
                   ("MENU2.PRG", "WrongOverlay"): 0x800F9800}
        self.assertEqual(counterpart_names(row, {}, symbols),
                         {"func_800F9800", "Renamed"})

    def test_template_bindings_have_a_real_checked_implementation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / r"Native\Game\src\Effects"
            source.mkdir(parents=True)
            (source / "Unit.c").write_text(
                '#define MACRO Effects_Unit\n#include "Effects/Unit.inl"\n', encoding="utf-8")
            body = source / "Unit.inl"
            body.write_text("void MACRO(void) { run(); }", encoding="utf-8")
            row = {"id": "F1", "kind": "template:unit:MACRO", "final_name": "Effects::Unit",
                   "final_location": r"Native\Game\src\Effects\Unit.c"}
            with patch.object(audit_module, "REPO", root):
                self.assertEqual(template_status(row)[0], "template-instance")
                row["kind"] = "INCLUDE_ASM"
                row["notes"] = "original source retained;native-template:MACRO"
                self.assertEqual(template_status(row)[0], "template-instance")
                body.write_text('void MACRO(void) { VS_UNPORTED("gap"); }', encoding="utf-8")
                self.assertEqual(template_status(row)[0], "stub")
                (source / "Unit.c").write_text(
                    '#define WRONG Effects_Unit\n#include "Effects/Unit.inl"\n',
                    encoding="utf-8",
                )
                with self.assertRaisesRegex(ValueError, "Missing template binding"):
                    template_status(row)

    def test_probe_mocks_are_excluded_from_production_c_availability(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for location in (r"pc\tests", r"pc\src"):
                (root / location).mkdir(parents=True)
            (root / r"pc\tests\probe.c").write_text(
                "int target(void) { return 0; }", encoding="utf-8")
            (root / r"pc\src\actual.c").write_text(
                "int target(void) { return work(); }", encoding="utf-8")
            with patch.object(audit_module, "REPO", root):
                found, _, _ = source_index(root, {"target"})
            self.assertEqual(len(found[None, "target"]), 1)
            self.assertIn("actual.c", found[None, "target"][0])

    def test_resource_output_is_rejected_before_any_write(self):
        with self.assertRaises(ValueError):
            audit(audit_module.REPO / "resources" / "forbidden")


if __name__ == "__main__":
    unittest.main()
