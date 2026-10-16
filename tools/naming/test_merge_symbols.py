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

    def make_type_batch(self, type_rows, field_rows):
        type_cols = merge.HEADERS["types"] + ["named"]
        field_cols = merge.HEADERS["fields"] + ["named", "type_id", "index"]
        self.write(self.ledgers / "TYPE_LEDGER.csv", type_cols,
                   [{"id": t} for t in ("T00001", "T00002")])
        self.write(self.ledgers / "FIELD_LEDGER.csv", field_cols, [
            {"id": "T00001.000", "type_id": "T00001", "index": "0"},
            {"id": "T00002.000", "type_id": "T00002", "index": "0"},
        ])
        self.write(self.batches / "Types" / "globals.csv", ["id"], [])
        self.write(self.batches / "Types" / "types.csv", ["id"],
                   [{"id": r["id"]} for r in type_rows])
        self.write(self.batches / "Types" / "fields.csv", ["id", "value"],
                   [{"id": r["id"], "value": ""} for r in field_rows])
        self.write(self.outs / "Types" / "types.csv", merge.HEADERS["types"], type_rows)
        self.write(self.outs / "Types" / "fields.csv", merge.HEADERS["fields"], field_rows)

    @staticmethod
    def type_row(tid, name, notes=""):
        return {"id": tid, "role": "Value", "final_name": name,
                "final_location": r"Native\Game\include\State\Time.h",
                "purpose": "Stores time.", "confidence": "high", "notes": notes}

    @staticmethod
    def field_row(fid, name):
        return {"id": fid, "final_name": name, "meaning": "Frames.", "confidence": "high", "notes": ""}

    def test_same_as_types_share_one_name(self):
        self.make_type_batch(
            [self.type_row("T00001", "State::Time"),
             self.type_row("T00002", "State::Time", "same-as:T00001")],
            [self.field_row("T00001.000", "frames"), self.field_row("T00002.000", "frames")])
        result, output = self.run_merge("--check", "Types")
        self.assertEqual(result, 0, output)

    def test_same_as_type_name_mismatch_is_rejected(self):
        self.make_type_batch(
            [self.type_row("T00001", "State::Time"),
             self.type_row("T00002", "State::Clock", "same-as:T00001")],
            [self.field_row("T00001.000", "frames"), self.field_row("T00002.000", "frames")])
        result, output = self.run_merge("--check", "Types")
        self.assertEqual(result, 1)
        self.assertIn("differs from same-as T00001", output)

    def test_same_as_field_name_mismatch_is_rejected(self):
        self.make_type_batch(
            [self.type_row("T00001", "State::Time"),
             self.type_row("T00002", "State::Time", "same-as:T00001")],
            [self.field_row("T00001.000", "frames"), self.field_row("T00002.000", "ticks")])
        result, output = self.run_merge("--check", "Types")
        self.assertEqual(result, 1)
        self.assertIn("differs from same-as T00001.000", output)

    def test_same_as_unknown_target_is_rejected(self):
        self.make_type_batch(
            [self.type_row("T00001", "State::Time"),
             self.type_row("T00002", "State::Time", "same-as:T09999")],
            [self.field_row("T00001.000", "frames"), self.field_row("T00002.000", "frames")])
        result, output = self.run_merge("--check", "Types")
        self.assertEqual(result, 1)
        self.assertIn("T09999 is not in TYPE_LEDGER", output)


if __name__ == "__main__":
    unittest.main()
