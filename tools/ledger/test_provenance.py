"""Synthetic controls for the decomp provenance check; no repository files are read."""

from __future__ import annotations

import csv
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import provenance  # noqa: E402

PREVIOUS_SOURCE = """#include "common.h"

// two lines the current decomp dropped
// from the top of the file

int keep(int x)
{
    return x;
}

void func_80010040(void)
{
}
"""
CURRENT_SOURCE = """#include "common.h"

int keep(int x)
{
    return x;
}

void renamedFn(void)
{
}
"""
GONE_SOURCE = """
int oldOnly(void)
{
    return 0;
}
"""
NATIVE = (
    "// decomp: keep (src/GAME/a.c:3) is current.\r\n"
    "// decomp: keep (src/GAME/a.c:8) is a stale line.\r\n"
    "// decomp: func_80010040 (a.c:11) was renamed.\r\n"
    "// decomp: oldOnly (src/GAME/gone.c:2) is in the previous copy only.\r\n"
    "int native_value;\r\n"
)
CATALOG_FIELDS = ["id", "overlay", "unit", "file", "line", "name", "kind", "notes"]


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.current = root / "decomp"
        self.previous = root / "previous"
        self.repo = root / "repo"
        self.write(self.current / "src/GAME/a.c", CURRENT_SOURCE)
        self.write(self.current / "config/GAME/symbol_addrs.txt", "renamedFn = 0x80010040;\n")
        self.write(self.previous / "src/GAME/a.c", PREVIOUS_SOURCE)
        self.write(self.previous / "src/GAME/gone.c", GONE_SOURCE)
        self.write(self.previous / "config/GAME/symbol_addrs.txt", "")
        self.native = self.repo / "Native/Game/src/Sample.cpp"
        self.native.parent.mkdir(parents=True, exist_ok=True)
        self.native.write_bytes(NATIVE.encode("utf-8"))
        self.catalog = self.repo / provenance.CATALOG
        self.catalog.parent.mkdir(parents=True, exist_ok=True)
        with self.catalog.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=CATALOG_FIELDS, lineterminator="\r\n")
            writer.writeheader()
            writer.writerow({"id": "F00001", "overlay": "GAME", "unit": "GAME", "file": "GAME/a.c", "line": "11",
                             "name": "func_80010040", "kind": "C", "notes": ""})
            writer.writerow({"id": "F00002", "overlay": "GAME", "unit": "GAME", "file": "GAME/a.c", "line": "3",
                             "name": "keep", "kind": "C", "notes": ""})

    @staticmethod
    def write(path: Path, text: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")

    def run(self, sync: bool = False, previous: bool = True) -> tuple[int, str]:
        out = io.StringIO()
        code = provenance.run(self.repo, self.current, self.previous if previous else None, sync, out)
        return code, out.getvalue()

    def unresolved(self) -> list[dict[str, str]]:
        with (self.repo / provenance.UNRESOLVED).open(encoding="utf-8", newline="") as stream:
            return list(csv.DictReader(stream))

    def catalog_rows(self) -> dict[str, dict[str, str]]:
        with self.catalog.open(encoding="utf-8", newline="") as stream:
            return {row["id"]: row for row in csv.DictReader(stream)}


class ProvenanceTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.fixture = Fixture(Path(self.temp.name))

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_check_reports_stale_lines_missing_files_and_catalog_rows(self):
        code, report = self.fixture.run()
        self.assertEqual(code, 1)
        self.assertNotIn("Sample.cpp:1:", report)
        self.assertIn("Sample.cpp:2: stale citation keep src/GAME/a.c:8", report)
        self.assertIn("Sample.cpp:3: stale citation func_80010040 a.c:11", report)
        self.assertIn("Sample.cpp:4: stale citation oldOnly src/GAME/gone.c:2", report)
        self.assertIn("FUNCTION_CATALOG.csv:F00001: stale citation func_80010040", report)
        self.assertNotIn("F00002", report)

    def test_sync_maps_by_identity_and_lists_the_unresolvable(self):
        code, _ = self.fixture.run(sync=True)
        self.assertEqual(code, 0)
        text = self.fixture.native.read_bytes().decode("utf-8")
        self.assertEqual(text.count("\r\n"), text.count("\n"))
        self.assertIn("// decomp: keep (src/GAME/a.c:3) is current.", text)
        self.assertIn("// decomp: keep (src/GAME/a.c:5) is a stale line.", text)
        self.assertIn("// decomp: renamedFn (src/GAME/a.c:8; was func_80010040) was renamed.", text)
        self.assertIn("// decomp: oldOnly (src/GAME/gone.c:2) is in", text)
        self.assertIn("int native_value;", text)
        rows = self.fixture.unresolved()
        self.assertEqual([(row["file"], row["name"], row["cited"]) for row in rows],
                         [("Native/Game/src/Sample.cpp", "oldOnly", "src/GAME/gone.c:2")])
        self.assertIn("previous copy only", rows[0]["reason"])
        catalog = self.fixture.catalog_rows()
        self.assertEqual((catalog["F00001"]["line"], catalog["F00001"]["name"], catalog["F00001"]["previous_name"]),
                         ("8", "renamedFn", "func_80010040"))
        self.assertEqual(catalog["F00002"]["previous_name"], "")
        code, report = self.fixture.run()
        self.assertEqual(code, 0, report)
        _, report = self.fixture.run(sync=True)
        self.assertIn("rewrote 0 citations", report)

    def test_check_fails_on_ledger_rows_that_resolve_or_match_nothing(self):
        self.fixture.run(sync=True)
        path = self.fixture.repo / provenance.UNRESOLVED
        rows = self.fixture.unresolved()
        rows.append({"file": "Native/Game/src/Sample.cpp", "line": "1", "name": "keep", "cited": "src/GAME/a.c:3",
                     "reason": "listed by mistake"})
        rows.append({"file": "Native/Game/src/Gone.cpp", "line": "1", "name": "gone", "cited": "src/GAME/a.c:1",
                     "reason": "citation deleted"})
        provenance.write_csv_rows(path, provenance.UNRESOLVED_FIELDS, rows, "\r\n")
        code, report = self.fixture.run()
        self.assertEqual(code, 1)
        self.assertIn("src/GAME/a.c:3 resolves now", report)
        self.assertIn("Gone.cpp gone src/GAME/a.c:1 matches no citation", report)

    def test_check_without_a_previous_tree_validates_the_rewritten_citations(self):
        self.fixture.run(sync=True)
        code, report = self.fixture.run(previous=False)
        self.assertEqual(code, 0, report)

    def test_missing_decomp_root_fails(self):
        self.fixture.current = self.fixture.root / "absent"
        with self.assertRaises(SystemExit):
            self.fixture.run()


if __name__ == "__main__":
    unittest.main()
