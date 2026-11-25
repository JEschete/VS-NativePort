"""Controls for the Markdown size budgets (doc_budget.py)."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import doc_budget


class DocBudgetTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)

    def tearDown(self) -> None:
        self.directory.cleanup()

    def write(self, path: str, data: bytes) -> str:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        return path

    def test_crlf_line_endings_are_measured_as_lf(self) -> None:
        path = self.write("Docs/A.md", b"x\r\n" * (doc_budget.DEFAULT_BUDGET // 2))
        self.assertEqual(doc_budget.check(self.root, [path]), ([], []))

    def test_a_document_over_its_budget_is_reported(self) -> None:
        path = self.write("Docs/A.md", b"x" * (doc_budget.DEFAULT_BUDGET + 1))
        violations, _ = doc_budget.check(self.root, [path])
        self.assertEqual(len(violations), 1)
        self.assertIn("Docs/A.md", violations[0])

    def test_a_specific_budget_overrides_the_default(self) -> None:
        handoff = self.write("Docs/HANDOFF.md", b"x" * (doc_budget.BUDGETS["Docs/HANDOFF.md"] + 1))
        legacy = self.write("Docs/OLD.md", b"x" * (doc_budget.DEFAULT_BUDGET + 1))
        with mock.patch.dict(doc_budget.LEGACY_CEILINGS, {"Docs/OLD.md": doc_budget.DEFAULT_BUDGET + 1}):
            violations, notes = doc_budget.check(self.root, [handoff, legacy])
        self.assertEqual([line.split(":")[0] for line in violations], ["Docs/HANDOFF.md"])
        self.assertEqual(notes, [])

    def test_a_legacy_document_that_fits_the_default_asks_to_drop_its_ceiling(self) -> None:
        path = self.write("Docs/OLD.md", b"x" * 10)
        with mock.patch.dict(doc_budget.LEGACY_CEILINGS, {"Docs/OLD.md": doc_budget.DEFAULT_BUDGET * 2}):
            violations, notes = doc_budget.check(self.root, [path])
        self.assertEqual(violations, [])
        self.assertEqual(len(notes), 1)

    def test_reference_documents_are_not_listed(self) -> None:
        self.write("Docs/A.md", b"a")
        self.write("resources/Ref/B.md", b"b")
        self.write("Docs/notes.txt", b"c")
        subprocess.run(["git", "init", "-q"], cwd=self.root, check=True)
        self.assertEqual(doc_budget.markdown_files(self.root), ["Docs/A.md"])


if __name__ == "__main__":
    unittest.main()
