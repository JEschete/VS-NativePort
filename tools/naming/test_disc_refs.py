"""Tests for resolving disc_refs.py targets from DATA_LEDGER rows."""
from __future__ import annotations

import csv
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import disc_refs  # noqa: E402

FIELDS = ["id", "name", "segment", "address", "size", "final_name"]
ROWS = [
    {"id": "G1", "name": "D_8005FFB0", "segment": "main", "address": "0x8005FFB0", "size": "2", "final_name": "kOne"},
    {"id": "G2", "name": "D_801096F0", "segment": "c-defined", "address": "", "size": "48", "final_name": ""},
    {"id": "G3", "name": "_ashleyRenderState", "segment": "c-defined", "address": "", "size": "4", "final_name": ""},
]


class RangesFromTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        ledger = Path(directory.name) / "DATA_LEDGER.csv"
        with ledger.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(ROWS)
        patch = mock.patch.object(disc_refs, "LEDGER", ledger)
        patch.start()
        self.addCleanup(patch.stop)

    def test_recorded_address(self):
        self.assertEqual(disc_refs.ranges_from(["G1"]), [("G1 kOne", 0x8005FFB0, 2)])

    def test_decomp_name_supplies_missing_address(self):
        self.assertEqual(disc_refs.ranges_from(["G2"]), [("G2 D_801096F0", 0x801096F0, 48)])

    def test_named_static_without_address_is_refused(self):
        with self.assertRaisesRegex(ValueError, r"G3 _ashleyRenderState has no recorded address"):
            disc_refs.ranges_from(["G3"])

    def test_unknown_id_is_refused(self):
        with self.assertRaisesRegex(ValueError, r"G9 is not a DATA_LEDGER id"):
            disc_refs.ranges_from(["G9"])

    def test_command_line_reports_refusal_without_traceback(self):
        with mock.patch.object(sys, "argv", ["disc_refs.py", "G3"]), \
                mock.patch("sys.stderr") as stderr, self.assertRaises(SystemExit) as raised:
            disc_refs.main()
        self.assertEqual(raised.exception.code, 2)
        written = "".join(call.args[0] for call in stderr.write.call_args_list)
        self.assertIn("has no recorded address", written)


if __name__ == "__main__":
    unittest.main()
