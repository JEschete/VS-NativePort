"""Terminal CASE metadata and listing work without images or optional Capstone."""

from __future__ import annotations

import builtins
import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest import mock


class StatusTerminalMetadataTests(unittest.TestCase):
    def test_metadata_and_listing_without_proof_dependencies(self):
        case_directory = Path(__file__).resolve().parent
        identifiers = (2675, 2676, 2677, 2678, 2679, 2682)
        paths = [
            case_directory / f"status_terminal_f{identifier:05}.py"
            for identifier in identifiers
        ]
        original_import = builtins.__import__

        def guarded_import(name, *args, **kwargs):
            if name == "capstone" or name.startswith("capstone."):
                raise AssertionError("metadata imported optional Capstone")
            return original_import(name, *args, **kwargs)

        with (
            mock.patch.object(sys, "path", [str(case_directory.parent), *sys.path]),
            mock.patch("builtins.__import__", side_effect=guarded_import),
            mock.patch.object(
                Path,
                "read_bytes",
                side_effect=AssertionError("metadata read image bytes"),
            ),
        ):
            import differential

            with mock.patch.object(
                differential.Image,
                "load",
                side_effect=AssertionError("metadata loaded image"),
            ):
                for identifier, path in zip(identifiers, paths, strict=True):
                    with self.subTest(identifier=identifier):
                        case = differential.load_case(str(path))
                        self.assertEqual(case.ID, f"F{identifier:05}")
                        self.assertGreater(case.END, case.ENTRY)
                        self.assertEqual(case.VECTORS, 1024)
                        self.assertTrue(case.MUTANTS)
                output = io.StringIO()
                cases = mock.Mock()
                cases.glob.return_value = paths
                with (
                    mock.patch.object(differential, "CASES", cases),
                    contextlib.redirect_stdout(output),
                ):
                    self.assertEqual(differential.main(["--list"]), 0)
                self.assertEqual(
                    [
                        line.split(" - ", 1)[0]
                        for line in output.getvalue().splitlines()
                    ],
                    [path.stem for path in paths],
                )


if __name__ == "__main__":
    unittest.main()
