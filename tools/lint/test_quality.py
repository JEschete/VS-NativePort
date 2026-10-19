"""Regression tests for quality baselines and real tool positive controls."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import quality


class BaselineTests(unittest.TestCase):
    def test_replacement_policy_keeps_safety_and_owned_gameplay_visible(self) -> None:
        style = quality.Finding(
            "tidy",
            "readability-magic-numbers",
            "Native/Engine/src/Core/Gte.cpp",
            1,
            "legacy register mask",
            "registers &= 31;",
        )
        safety = quality.Finding(
            "tidy",
            "clang-analyzer-core.NullDereference",
            style.path,
            1,
            "null dereference",
            "*pointer = 1;",
        )
        gameplay = quality.Finding(
            style.tool,
            style.rule,
            "Native/Game/src/Actors/SequencePlayback.c",
            style.line,
            style.message,
            style.context,
        )
        self.assertTrue(quality.deferred_replacement_style(style))
        self.assertFalse(quality.deferred_replacement_style(safety))
        self.assertFalse(quality.deferred_replacement_style(gameplay))

    def test_fixture_policy_never_exempts_production_or_safety_findings(self) -> None:
        fixture = quality.Finding(
            "tidy",
            "readability-magic-numbers",
            "Native/Tests/src/Fixture.c",
            1,
            "expected pixel",
            "CHECK(pixel == 255);",
        )
        production = quality.Finding(
            fixture.tool,
            fixture.rule,
            "Native/Game/src/Battle/ActionResults.c",
            fixture.line,
            fixture.message,
            fixture.context,
        )
        safety = quality.Finding(
            "tidy",
            "clang-analyzer-core.NullDereference",
            fixture.path,
            1,
            "null dereference",
            "*pointer = 1;",
        )
        self.assertTrue(quality.intentional_test_fixture(fixture))
        self.assertFalse(quality.intentional_test_fixture(production))
        self.assertFalse(quality.intentional_test_fixture(safety))

    def test_finding_areas_do_not_exempt_owned_c_code_or_tests(self) -> None:
        for path, expected in (
            ("Native/Engine/Legacy/Render/LibGpu.h", "legacy"),
            ("Native/Tests/src/LegacyGpuTests.cpp", "tests"),
            ("Native/Game/src/Battle/ActionResults.c", "native"),
            ("Native/Engine/src/Core/Gte.cpp", "native"),
            ("tools/port/copy_fixups.py", "tooling"),
        ):
            item = quality.Finding("tidy", "rule", path, 1, "message", "code")
            self.assertEqual(item.area, expected)
            self.assertEqual(quality.new_findings([item], {}), [item])

    def test_existing_finding_survives_a_line_move(self) -> None:
        original = quality.Finding(
            "tidy", "rule", "Native/example.cc", 4, "message", "code"
        )
        moved = quality.Finding(
            "tidy", "rule", "Native/example.cc", 19, "message", "code"
        )
        self.assertEqual(quality.new_findings([moved], {original.key: 1}), [])

    def test_an_additional_occurrence_is_not_baselined(self) -> None:
        item = quality.Finding(
            "tidy", "rule", "Native/example.cc", 4, "message", "code"
        )
        self.assertEqual(quality.new_findings([item, item], {item.key: 1}), [item])

    def test_distinct_clone_occurrences_are_not_deduplicated(self) -> None:
        clone = quality.Finding(
            "clones", "cpp", "Native/example.cc", 4, "clone", "body"
        )
        found = quality.deduplicate_findings([clone, clone])
        self.assertEqual(quality.new_findings(found, {clone.key: 1}), [clone])

    def test_new_code_is_not_hidden_by_the_same_diagnostic_count(self) -> None:
        old = quality.Finding(
            "tidy", "rule", "Native/example.cc", 4, "message", "old code"
        )
        new = quality.Finding(
            "tidy", "rule", "Native/example.cc", 4, "message", "new code"
        )
        self.assertEqual(quality.new_findings([new], {old.key: 1}), [new])

    def test_format_reconciliation_preserves_tokens_and_occurrence_limits(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Native" / "example.cc"
            source.parent.mkdir()
            original = quality.Finding(
                "tidy",
                "rule",
                "Native/example.cc",
                1,
                "message",
                "int Calculate(int input) { return input + 37; }",
            )
            baseline = {
                "schema": 1,
                "findings": [
                    {
                        "key": original.key,
                        "count": 1,
                        "tool": original.tool,
                        "path": original.path,
                        "rule": original.rule,
                    }
                ],
            }
            source.write_text("int Calculate(int input) {\n  return input + 37;\n}\n")
            wrapped = quality.Finding(
                "tidy", "rule", original.path, 2, "message", "return input + 37;"
            )
            with patch.object(quality, "ROOT", root):
                migrated = quality.reconcile_formatting(
                    [wrapped, wrapped], baseline, [original]
                )
            counts = {row["key"]: row["count"] for row in migrated["findings"]}
            self.assertEqual(
                quality.new_findings([wrapped, wrapped], counts), [wrapped]
            )
            self.assertEqual(sum(counts.values()), 1)
            self.assertEqual(baseline["findings"][0]["key"], original.key)

            source.write_text("int Calculate(int input) {\n  return input - 37;\n}\n")
            changed = quality.Finding(
                "tidy", "rule", original.path, 2, "message", "return input - 37;"
            )
            with patch.object(quality, "ROOT", root):
                rejected = quality.reconcile_formatting([changed], baseline, [original])
            self.assertEqual(rejected, baseline)

    def test_format_reconciliation_preserves_string_contents(self) -> None:
        first = quality.native_tokens("first.cc", 'const char* text = "two words";')
        second = quality.native_tokens("second.cc", 'const char *text="twowords";')
        self.assertNotEqual(
            [token[0] for token in first], [token[0] for token in second]
        )

    def test_format_reconciliation_only_expands_the_same_unchanged_region(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Native" / "example.cc"
            source.parent.mkdir()
            original = quality.Finding(
                "tidy",
                "rule",
                "Native/example.cc",
                1,
                "message",
                "long First(); long Second();",
            )
            baseline = {
                "schema": 1,
                "findings": [
                    {
                        "key": original.key,
                        "count": 1,
                        "tool": original.tool,
                        "path": original.path,
                        "rule": original.rule,
                    }
                ],
            }
            source.write_text(
                "long First();\nlong Second();\nlong First();\nlong Second();\n"
            )
            findings = [
                quality.Finding("tidy", "rule", original.path, index, "message", text)
                for index, text in enumerate(source.read_text().splitlines(), 1)
            ]
            with patch.object(quality, "ROOT", root):
                migrated = quality.reconcile_formatting(findings, baseline, [original])
            counts = {row["key"]: row["count"] for row in migrated["findings"]}
            self.assertEqual(quality.new_findings(findings, counts), findings[2:])

    def test_scope_excludes_copied_and_external_sources(self) -> None:
        native, python = quality.discover_files()
        self.assertIn("Native/Engine/src/Core/FrameClock.cpp", native)
        self.assertNotIn("Native/Game/src/Battle/Targeting.c", native)
        self.assertTrue(all(path.startswith("Native/") for path in native))
        self.assertFalse(
            any(
                "resources/" in path or "node_modules/" in path
                for path in native + python
            )
        )

    def test_header_edits_are_analyzed_through_translation_units(self) -> None:
        files = [
            "Native/Engine/src/first.cpp",
            "Native/Engine/src/second.c",
            "Native/Engine/include/api.h",
        ]
        self.assertEqual(quality.tidy_sources(files, {files[2]}), files[:2])
        self.assertEqual(quality.tidy_sources(files, {files[0]}), [files[0]])


class ToolControlTests(unittest.TestCase):
    def setUp(self) -> None:
        quality.OUTPUT.mkdir(parents=True, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(
            prefix=".quality-control-", dir=quality.ROOT / "Native"
        )
        self.addCleanup(self.directory.cleanup)

    def source(self, name: str, content: str) -> str:
        path = Path(self.directory.name) / name
        path.write_text(content, encoding="utf-8")
        return quality.relative(path)

    def test_formatter_and_cpplint_reject_bad_style(self) -> None:
        path = self.source(
            "format_control.cc",
            "namespace quality_control {\nint Add(int left,int right){return left+right;}\n}\n",
        )
        self.assertTrue(quality.check_format([path]))
        self.assertTrue(quality.check_cpplint([path]))
        formatted = quality.run(
            [quality.llvm_tool("clang-format"), "--style=file", path]
        )
        (quality.ROOT / path).write_text(formatted, encoding="utf-8")
        self.assertEqual(quality.check_format([path]), [])

    def test_catch2_checks_do_not_request_logging_check_macros(self) -> None:
        with tempfile.TemporaryDirectory(
            dir=quality.ROOT / "Native/Tests"
        ) as directory:
            source = Path(directory) / "catch_control.cpp"
            source.write_text("void Test() { CHECK(value == 0); }\n", encoding="utf-8")
            findings = quality.check_cpplint([quality.relative(source)])
        self.assertFalse(any(item.rule == "readability/check" for item in findings))

    def test_clang_tidy_detects_an_unnamed_gameplay_number(self) -> None:
        path = self.source("number_control.cc", "int CalculateValue() { return 37; }\n")
        findings = quality.check_tidy([path], {path})
        self.assertTrue(
            any(item.rule == "readability-magic-numbers" for item in findings)
        )

    def test_clang_tidy_detects_an_actual_null_dereference(self) -> None:
        path = self.source(
            "null_control.cc",
            "int ReadMissingValue() { int* value = nullptr; return *value; }\n",
        )
        findings = quality.check_tidy([path], {path})
        self.assertTrue(
            any(item.rule == "clang-analyzer-core.NullDereference" for item in findings)
        )

    def test_missing_cpplint_is_an_infrastructure_failure(self) -> None:
        path = self.source("missing_tool.cc", "int CalculateValue() { return 0; }\n")
        with (
            patch.object(quality, "run", return_value="No module named cpplint"),
            self.assertRaisesRegex(RuntimeError, "did not process"),
        ):
            quality.check_cpplint([path])

    def test_cli_rejects_new_debt_without_updating_the_baseline(self) -> None:
        before = quality.BASELINE.read_bytes()
        path = self.source("new_debt.cc", "int CalculateValue() { return 37; }\n")
        result = subprocess.run(
            [
                sys.executable,
                str(quality.HERE / "quality.py"),
                "--files",
                path,
                "--tidy",
            ],
            cwd=quality.ROOT,
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
        )
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("readability-magic-numbers", result.stdout)
        self.assertEqual(quality.BASELINE.read_bytes(), before)

    def test_ruff_detects_an_undefined_name(self) -> None:
        path = self.source("python_control.py", "print(missing_value)\n")
        self.assertTrue(any(item.rule == "F821" for item in quality.check_ruff([path])))

    def test_clone_detector_analyzes_the_supplied_files(self) -> None:
        body = "int CalculateValue(int input) {\n  int result = input;\n"
        body += "".join(f"  result += input * {number};\n" for number in range(10, 30))
        body += "  return result;\n}\n"
        first = self.source("clone_first.cc", body)
        second = self.source("clone_second.cc", body)
        findings = quality.check_clones([first, second])
        self.assertTrue(findings)
        self.assertTrue(
            any(
                first in (item.path, item.message) or second in item.message
                for item in findings
            )
        )


if __name__ == "__main__":
    unittest.main()
