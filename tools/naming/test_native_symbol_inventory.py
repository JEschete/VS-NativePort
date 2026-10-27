"""Exact-snapshot review gates must not turn inventories into approvals."""

import csv
import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from tools.naming import native_symbol_inventory as inventory


class NativeSymbolInventoryTests(unittest.TestCase):
    def test_inline_headers_and_owned_paths_are_all_scanned(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tools" / "port").mkdir(parents=True)
            source = root / "Native" / "Engine"
            source.mkdir(parents=True)
            paths = [source / ("sample" + suffix) for suffix in (".c", ".h", ".inl", ".tpp", ".ixx")]
            for path in paths:
                path.write_text("int arg0;", encoding="utf-8")
            (source / "not-source.txt").write_text("int arg0;", encoding="utf-8")
            manifest = {"\\".join(path.relative_to(root).parts): "fixture" for path in paths[:3]}
            (root / "tools" / "port" / "copy_manifest.json").write_text(json.dumps(manifest))
            owned_path = "\\".join(paths[0].relative_to(root).parts)
            with (root / "tools" / "port" / "owned.csv").open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=("path", "header"))
                writer.writeheader()
                writer.writerow({"path": owned_path, "header": ""})
            first = inventory.scan(root)
            self.assertEqual(first, inventory.scan(root))
            candidates, sources = first
            self.assertEqual(len(sources), 5)
            self.assertEqual(len(candidates), 5)
            by_suffix = {Path(item.path).suffix: item for item in candidates}
            self.assertEqual(by_suffix[".c"].ownership, "registered-owned")
            self.assertEqual(by_suffix[".h"].ownership, "copier-managed")
            self.assertEqual(by_suffix[".tpp"].ownership, "maintained")
            self.assertEqual(by_suffix[".inl"].source_kind, "inline")
            self.assertEqual(by_suffix[".ixx"].source_kind, "implementation")

    def test_literals_are_excluded_but_false_positives_stay_visible(self):
        source = (
            "// arg0\n"
            'auto s = R"tag(temp_v0 1\'000)tag";\n'
            "int n = 1'000; int speed = 0; int arg0 = 2'000;\n"
            "std::filesystem::temp_directory_path();\n"
        )
        found = inventory.occurrences("fixture.cpp", source, "maintained", ".cpp")
        self.assertEqual([item.identifier for item in found], ["speed", "arg0", "temp_directory_path"])
        self.assertEqual(found[1].line, 3)
        self.assertEqual(source.splitlines()[2][found[1].column - 1:][:4], "arg0")

    def test_line_endings_do_not_change_snapshot_identity_or_positions(self):
        source = "int arg0;\nint arg1;\n"
        self.assertEqual(
            inventory.occurrences("fixture.h", source, "maintained", ".h"),
            inventory.occurrences("fixture.h", source.replace("\n", "\r\n"), "maintained", ".h"),
        )

    def test_every_occurrence_needs_its_own_approved_position(self):
        candidates = inventory.occurrences("fixture.c", "int arg0 = arg0;", "maintained", ".c")
        sources = {"fixture.c": candidates[0].source_sha256}
        report = inventory.reconcile(candidates, sources, {candidates[0].key: {"binding": "fixture"}})
        self.assertEqual(report["reviewed_occurrences"], 1)
        self.assertEqual(len(report["unreviewed"]), 1)

    def test_changed_source_cannot_reuse_old_review(self):
        old = inventory.occurrences("fixture.c", "int arg0 = 1;", "maintained", ".c")[0]
        new = inventory.occurrences("fixture.c", "int arg0 = 2;", "maintained", ".c")
        report = inventory.reconcile(new, {"fixture.c": new[0].source_sha256}, {old.key: {}})
        self.assertEqual(report["reviewed_occurrences"], 0)
        self.assertEqual(report["inactive_snapshot_positions"], 1)
        self.assertEqual(len(report["unreviewed"]), 1)

    def test_extra_active_review_position_is_an_error_not_an_approval(self):
        item = inventory.occurrences("fixture.c", "int arg0;", "maintained", ".c")[0]
        extra = replace(item, column=item.column + 1)
        report = inventory.reconcile(
            [item], {"fixture.c": item.source_sha256}, {item.key: {}, extra.key: {}}
        )
        self.assertEqual(report["reviewed_occurrences"], 1)
        self.assertEqual(len(report["extra_review_positions"]), 1)

    def test_review_loader_requires_specific_evidence_and_unique_positions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "reviews.csv"
            row = {
                "path": "fixture.c", "source_sha256": "a" * 64, "identifier": "arg0",
                "binding": "Ffixture unused formal", "positions": "1:5",
                "rationale": "Declared but never read or forwarded.", "evidence": "original.c:10",
            }

            def write(rows):
                with path.open("w", newline="", encoding="utf-8") as stream:
                    writer = csv.DictWriter(stream, fieldnames=inventory.REVIEW_COLUMNS)
                    writer.writeheader()
                    writer.writerows(rows)

            write([row])
            self.assertEqual(len(inventory.read_reviews(path)), 1)
            write([row, row])
            with self.assertRaisesRegex(ValueError, "duplicate"):
                inventory.read_reviews(path)
            write([row | {"rationale": ""}])
            with self.assertRaisesRegex(ValueError, "incomplete"):
                inventory.read_reviews(path)
            write([row | {"positions": "0:5"}])
            with self.assertRaisesRegex(ValueError, "invalid position"):
                inventory.read_reviews(path)


if __name__ == "__main__":
    unittest.main()
