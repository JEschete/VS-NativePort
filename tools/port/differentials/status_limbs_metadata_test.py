"""Metadata listing must not import optional Capstone or touch original images."""

from __future__ import annotations

import builtins
import sys
import unittest
from pathlib import Path
from unittest import mock


class StatusLimbsMetadataTests(unittest.TestCase):
    def test_metadata_without_optional_proof_dependencies(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
        import differential

        original_import = builtins.__import__

        def guarded_import(name, *args, **kwargs):
            if name == "capstone" or name.startswith("capstone."):
                raise AssertionError("metadata imported optional Capstone")
            return original_import(name, *args, **kwargs)

        with (
            mock.patch("builtins.__import__", side_effect=guarded_import),
            mock.patch.object(
                Path,
                "read_bytes",
                side_effect=AssertionError("metadata read image bytes"),
            ),
            mock.patch.object(
                differential.Image,
                "load",
                side_effect=AssertionError("metadata loaded image"),
            ),
        ):
            for identifier in (2662, 2663, 2664, 2665, 2668, 2680, 2681):
                with self.subTest(identifier=identifier):
                    case = differential.load_case(f"status_limbs_f{identifier:05}")
                    self.assertEqual(case.ID, f"F{identifier:05}")
                    self.assertGreater(case.END, case.ENTRY)
                    self.assertGreaterEqual(case.VECTORS, 1024)
                    self.assertTrue(case.MUTANTS)


if __name__ == "__main__":
    unittest.main()
