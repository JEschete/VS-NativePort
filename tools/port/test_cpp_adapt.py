"""Compiler-diagnostic recovery controls for generated C++ conversions."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import cpp_adapt


class CppAdaptTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="cpp_adapt_test_")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.path = r"Native\sample.cpp"
        self.source = self.root / self.path
        self.source.parent.mkdir()
        self.original = 'void draw(signed char* text) {\n  sprintf(text, "%d", 3);\n}\n'
        self.source.write_text(self.original, encoding="utf-8")

    def derive(self, notes: str) -> tuple[int, int]:
        diagnostic = (
            f"{self.path}(2,3): error: no matching function for call to 'sprintf'\n" + notes
        )
        with (
            patch.object(cpp_adapt, "ROOT", self.root),
            patch.object(cpp_adapt, "compile_all", return_value=[(self.path, diagnostic)]),
        ):
            return cpp_adapt.derive_pass([self.path])

    def test_macro_expansion_notes_do_not_hide_unique_pointer_conversion(self) -> None:
        notes = (
            "stdio.h(1,1): note: candidate function not viable: no known conversion from "
            "'signed char[4]' to 'char *' for 1st argument\n"
            "corecrt.h(2,1): note: expanded from macro 'OVERLOAD'\n"
            "corecrt.h(3,1): note: expanded from macro 'OVERLOAD_EX'\n"
        )
        self.assertEqual(self.derive(notes), (1, 0))
        self.assertEqual(
            self.source.read_text(encoding="utf-8"),
            self.original.replace("sprintf(text,", "sprintf((char *)(text),"),
        )

    def test_multiple_candidate_conversions_remain_unresolved(self) -> None:
        notes = (
            "stdio.h(1,1): note: candidate function not viable: no known conversion from "
            "'signed char[4]' to 'char *' for 1st argument\n"
            "other.h(2,1): note: candidate function not viable: no known conversion from "
            "'signed char[4]' to 'wchar_t *' for 1st argument\n"
        )
        self.assertEqual(self.derive(notes), (0, 1))
        self.assertEqual(self.source.read_text(encoding="utf-8"), self.original)


if __name__ == "__main__":
    unittest.main()
