"""Regression tests for validated incremental symbol-ledger merges."""
from __future__ import annotations

import contextlib
import csv
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import merge_symbols as merge


class BatchMergeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.ledgers = root / "ledgers"
        self.batches = root / "batches"
        self.outs = root / "out"
        self.ledgers.mkdir()
        self.batches.mkdir()
        self.outs.mkdir()
        self.addCleanup(patch.stopall)
        patch.multiple(
            merge, LEDGERS=self.ledgers, BATCHES=self.batches,
            OUTS=self.outs, CATALOG=self.ledgers / "functions.csv",
        ).start()
        self.write(self.ledgers / "functions.csv", ["final_name"], [])
        for kind, filename in merge.LEDGER.items():
            extra = ["linkage"] if kind == "globals" else ["type_id"] if kind == "fields" else []
            cols = merge.HEADERS[kind] + ["named"] + extra
            rows = []
            if kind == "globals":
                rows = [
                    {"id": "G00001", "linkage": "extern", "notes": "keep until reviewed"},
                    {"id": "G00002", "linkage": "extern", "notes": "untouched"},
                ]
            self.write(self.ledgers / filename, cols, rows)
        self.make_batch("Title", "G00001")
        self.output = {
            "id": "G00001", "disposition": "state", "final_name": "Title::visitCount",
            "final_location": r"Native\Game\src\Title\TitleScreen.c",
            "purpose": "Counts title visits.", "confidence": "high", "notes": "state:Title::TitleState",
        }
        self.write(
            self.outs / "Title" / "globals.csv",
            merge.HEADERS["globals"], [self.output],
        )

    @staticmethod
    def write(path, cols, rows):
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=cols)
            writer.writeheader()
            writer.writerows(rows)

    def make_batch(self, name, gid):
        for kind in merge.HEADERS:
            self.write(
                self.batches / name / f"{kind}.csv", ["id"],
                [{"id": gid}] if kind == "globals" else [],
            )

    def run_merge(self, *args):
        with patch.object(sys, "argv", ["merge_symbols.py", *args]):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                result = merge.main()
        return result, output.getvalue()

    def snapshots(self):
        return {p.name: p.read_bytes() for p in self.ledgers.iterdir()}

    def test_complete_batch_applies_without_other_batches(self):
        self.make_batch("Other", "G00002")
        result, output = self.run_merge("--apply-batch", "Title")
        self.assertEqual(result, 0, output)
        rows = merge.read(self.ledgers / "DATA_LEDGER.csv")
        self.assertEqual(rows[0]["final_name"], "Title::visitCount")
        self.assertEqual(rows[0]["named"], "x")
        self.assertEqual(rows[1]["final_name"], "")
        self.assertEqual(rows[1]["notes"], "untouched")

    def test_incomplete_batch_does_not_write(self):
        self.write(
            self.batches / "Title" / "globals.csv", ["id"],
            [{"id": "G00001"}, {"id": "G00002"}],
        )
        before = self.snapshots()
        result, output = self.run_merge("--apply-batch", "Title")
        self.assertEqual(result, 1)
        self.assertIn("ids missing", output)
        self.assertEqual(self.snapshots(), before)

    def test_collision_with_applied_ledger_does_not_write(self):
        path = self.ledgers / "DATA_LEDGER.csv"
        rows = merge.read(path)
        rows[1]["final_name"] = self.output["final_name"]
        self.write(path, list(rows[0]), rows)
        before = self.snapshots()
        result, output = self.run_merge("--apply-batch", "Title")
        self.assertEqual(result, 1)
        self.assertIn("used by 2 rows", output)
        self.assertEqual(self.snapshots(), before)

    def test_unknown_batch_is_rejected(self):
        before = self.snapshots()
        result, output = self.run_merge("--apply-batch", "Missing")
        self.assertEqual(result, 1)
        self.assertIn("unknown batch", output)
        self.assertEqual(self.snapshots(), before)

    def test_missing_input_is_rejected(self):
        (self.batches / "Title" / "types.csv").unlink()
        before = self.snapshots()
        result, output = self.run_merge("--apply-batch", "Title")
        self.assertEqual(result, 1)
        self.assertIn("input missing", output)
        self.assertEqual(self.snapshots(), before)

    def test_apply_all_still_requires_other_batches(self):
        self.make_batch("Other", "G00002")
        before = self.snapshots()
        result, output = self.run_merge("--apply")
        self.assertEqual(result, 1)
        self.assertIn("output missing", output)
        self.assertEqual(self.snapshots(), before)


if __name__ == "__main__":
    unittest.main()
