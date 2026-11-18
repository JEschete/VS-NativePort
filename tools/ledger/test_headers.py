"""Synthetic controls for the C++ header migration ledger; no repository files are read."""

from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import headers  # noqa: E402

GENERATED = """// Copied from the decomp by tools/port/copy_to_native.py.
#ifndef VS_MENU_MAP_H_TYPES
#define VS_MENU_MAP_H_TYPES
typedef struct Menu_Map_Label { int x; } Menu_Map_Label;
_Static_assert(sizeof(Menu_Map_Label) == 4, "layout");
#endif
#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_MAP_H_DECLS)
#define VS_MENU_MAP_H_DECLS
void Menu_Map_Draw(void);
static int Menu_Map_unused __attribute__((unused));
#endif
"""
CPP = """#pragma once
#include <cstdint>
#define VS_ASSERT(cond) ((void)(cond))
struct SDL_Window;
enum class Order : std::uint8_t;
static_assert(sizeof(int) == 4);
namespace VS::Menu {
struct Point {
  int x;
  bool operator==(const Point&) const = default;
};
inline bool Same(Point a, Point b) { return a == b; }
}  // namespace VS::Menu
"""
LINKED = """#ifndef VS_LINKED_H
#define VS_LINKED_H
#define VS_LINKED_COUNT 4
#ifdef __cplusplus
extern "C" {
#endif
int Linked_Value(void);
#ifdef __cplusplus
}
#endif
#endif
"""


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.manifest: dict[str, str] = {}
        self.files: dict[str, str] = {}

    def write(self) -> headers.HeaderWorkspace:
        port = self.root / "tools" / "port"
        port.mkdir(parents=True, exist_ok=True)
        (port / "copy_manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        for path, text in self.files.items():
            full = self.root / path
            full.parent.mkdir(parents=True, exist_ok=True)
            full.write_text(text, encoding="utf-8")
        tracker = self.root / "Docs" / "CPP_HEADERS.md"
        if not tracker.exists():
            tracker.parent.mkdir(parents=True, exist_ok=True)
            tracker.write_text(f"# Headers\n\n{headers.SUMMARY_BEGIN}\n{headers.SUMMARY_END}\n", encoding="utf-8")
        (self.root / "Docs" / "ledgers").mkdir(parents=True, exist_ok=True)
        return headers.HeaderWorkspace(self.root)

    def remove(self, path: str) -> None:
        del self.files[path]
        (self.root / path).unlink()


def rules(workspace: headers.HeaderWorkspace) -> list[str]:
    return sorted(issue.rule for issue in headers.check(workspace))


def ledger(workspace: headers.HeaderWorkspace) -> dict[str, dict[str, str]]:
    with workspace.ledger.open(encoding="utf-8", newline="") as stream:
        return {row["path"]: row for row in csv.DictReader(stream)}


class MeasureTests(unittest.TestCase):
    def test_a_generated_header_is_measured_as_c(self) -> None:
        values = headers.measure(GENERATED)
        self.assertEqual(values["guard"], "split")
        self.assertEqual((values["typedefs"], values["static_asserts"], values["void_params"],
                          values["gnu_attributes"]), ("1", "1", "1", "1"))
        self.assertEqual(values["macros"], "0", "section guards are not macros")
        self.assertEqual(values["global_decls"], "3")

    def test_a_cpp_header_passes_every_count(self) -> None:
        values = headers.measure(CPP)
        self.assertEqual(values["guard"], headers.PRAGMA_ONCE)
        self.assertEqual({column: values[column] for column in headers.COUNTS}, dict.fromkeys(headers.COUNTS, "0"))

    def test_c_linkage_blocks_are_global_scope(self) -> None:
        values = headers.measure(LINKED)
        self.assertEqual(values["guard"], "macro")
        self.assertEqual(values["c_linkage"], "3")
        self.assertEqual(values["macros"], "1")
        self.assertEqual(values["global_decls"], "1")
        self.assertEqual(values["void_params"], "1")

    def test_wrapped_includes_skip_braces_in_literals_and_count_nested_blocks_once(self) -> None:
        text = ('const char* k = "{";\nextern "C" {\n#include "A.h"\nextern "C" {\n#include "B.h"\n}\n'
                'const char* f = "}}";\n#include "C.h"\n}\n#include "D.h"\n')
        self.assertEqual(headers.wrapped_includes(text), ["A.h", "B.h", "C.h"])


class LedgerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.fixture = Fixture(Path(self.directory.name))
        self.fixture.manifest = {"Native/Game/src/Menu/Map.h": "hash"}
        self.fixture.files = {
            "Native/Game/src/Menu/Map.h": GENERATED,
            "Native/Game/src/Menu/Linked.h": LINKED,
            "Native/Game/include/Menu/Point.hpp": CPP,
            "Native/Game/src/Menu/Uses.cpp": 'extern "C" {\n#include "Menu/Linked.h"\n}\n',
            "Native/Game/src/Menu/Plain.cpp": '#include "Menu/Point.hpp"\n',
        }
        self.workspace = self.fixture.write()
        headers.sync(self.workspace)

    def tearDown(self) -> None:
        self.directory.cleanup()

    def test_creation_tracks_c_files_only(self) -> None:
        rows = ledger(self.workspace)
        self.assertEqual(sorted(rows), ["Native/Game/src/Menu/Linked.h", "Native/Game/src/Menu/Map.h",
                                        "Native/Game/src/Menu/Uses.cpp"])
        self.assertEqual(rows["Native/Game/src/Menu/Map.h"]["origin"], "generated")
        self.assertEqual(rows["Native/Game/src/Menu/Linked.h"]["c_wrapped"], "1")
        self.assertEqual(rows["Native/Game/src/Menu/Uses.cpp"]["id"], "S0001")
        self.assertEqual(rules(self.workspace), [])

    def test_new_c_style_files_are_rejected(self) -> None:
        self.fixture.files["Native/Game/src/Menu/New.h"] = "#pragma once\nnamespace VS::Menu {}\n"
        self.fixture.files["Native/Game/src/Menu/New.cpp"] = 'extern "C" int New_Value() { return 1; }\n'
        self.fixture.write()
        self.assertEqual(rules(self.workspace), ["header-new", "source-new"])

    def test_include_roots_decide_which_header_is_wrapped(self) -> None:
        # A same-named header in the same project that no include root reaches is not the one the compiler opens.
        self.fixture.files["Native/Game/src/Other/Menu/Linked.h"] = CPP
        self.fixture.write()
        scanner = headers.Scanner(self.workspace)
        self.assertEqual(scanner.wrapped["Native/Game/src/Menu/Linked.h"], 1)
        self.assertEqual(scanner.wrapped["Native/Game/src/Other/Menu/Linked.h"], 0)

    def test_a_migration_is_followed_and_done_is_sticky(self) -> None:
        self.fixture.remove("Native/Game/src/Menu/Linked.h")
        self.fixture.files["Native/Game/src/Menu/Linked.hpp"] = CPP
        self.fixture.files["Native/Game/src/Menu/Uses.cpp"] = '#include "Menu/Linked.hpp"\n'
        self.fixture.write()
        self.assertIn("file-missing", rules(self.workspace))
        headers.sync(self.workspace)
        rows = ledger(self.workspace)
        self.assertEqual(rows["Native/Game/src/Menu/Linked.hpp"]["id"], "H0001")
        self.assertEqual(rows["Native/Game/src/Menu/Linked.hpp"]["status"], "done")
        self.assertEqual(rows["Native/Game/src/Menu/Uses.cpp"]["status"], "done")
        self.assertEqual(rules(self.workspace), [])

        # Wrapping the migrated header again regresses it and its includer.
        self.fixture.files["Native/Game/src/Menu/Uses.cpp"] = 'extern "C" {\n#include "Menu/Linked.hpp"\n}\n'
        self.fixture.write()
        self.assertEqual(rules(self.workspace), ["header-regressed", "ledger-stale", "ledger-stale",
                                                 "source-regressed"])
        headers.sync(self.workspace)
        self.assertEqual(ledger(self.workspace)["Native/Game/src/Menu/Linked.hpp"]["status"], "done")
        self.assertEqual(rules(self.workspace), ["header-regressed", "source-regressed"])

    def test_line_endings_do_not_change_a_measurement(self) -> None:
        text = "#pragma once\n#define VS_PAIR \\\n  int first; \\\n  int second;\nvoid Sum(void);\n"
        for name, ending in (("Lf.h", b"\n"), ("Crlf.h", b"\r\n")):
            (self.workspace.root / "Native" / name).write_bytes(text.encode().replace(b"\n", ending))
        scanner = headers.Scanner(self.workspace)
        self.assertEqual(scanner.values("Native/Crlf.h"), scanner.values("Native/Lf.h"))
        self.assertEqual(scanner.values("Native/Crlf.h")["global_decls"], "1")

    def test_retired_needs_a_deleted_file_and_a_note(self) -> None:
        rows = list(ledger(self.workspace).values())
        for row in rows:
            if row["path"].endswith("Map.h"):
                row["status"] = "retired"
        headers.write_ledger(self.workspace, rows)
        headers.write_tracker(self.workspace, rows)
        self.assertEqual(rules(self.workspace), ["retired-present", "retired-without-reason"])

    def test_an_edited_tracker_block_is_stale(self) -> None:
        text = self.workspace.tracker.read_text(encoding="utf-8")
        self.workspace.tracker.write_text(text.replace("headers done", "headers finished"), encoding="utf-8")
        self.assertEqual(rules(self.workspace), ["tracker-stale"])


if __name__ == "__main__":
    unittest.main()
