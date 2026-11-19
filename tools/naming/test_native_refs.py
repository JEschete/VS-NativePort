"""Tests for the libclang reference walker in native_refs.py."""
from __future__ import annotations

import itertools
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import native_refs  # noqa: E402

SOURCE = """\
namespace VS::Audio {
struct Record { int pinned; int bound; struct Inner { int nested; }; };
}
struct Flat { int flat; };
unsigned long long Bounds() {
  return __builtin_offsetof(Flat, flat) + __builtin_offsetof(VS::Audio::Record, bound) +
         __builtin_offsetof(VS::Audio::Record::Inner, nested);
}
static_assert(__builtin_offsetof(VS::Audio::Record, pinned) == 0);
"""


class LibclangTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            native_refs.configure(native_refs.libclang_path())
        except Exception as error:  # noqa: BLE001 - any failure to load libclang means the walker cannot run
            raise unittest.SkipTest(f"libclang is unavailable: {error}") from error


class OffsetofTests(LibclangTest):
    def accesses(self, source: str) -> dict[str, set[str]]:
        with tempfile.TemporaryDirectory() as root:
            path = os.path.join(root, "Unit.cpp")
            Path(path).write_text(source, encoding="utf-8")
            native_refs.relative.cache_clear()
            try:
                with mock.patch.object(native_refs, "NATIVE_PREFIX", os.path.normcase(root) + os.sep):
                    walker = native_refs.Walker()
                    errors = walker.unit(path, ["-x", "c++", "-std=c++20"], native_refs.ci.Index.create())
            finally:
                native_refs.relative.cache_clear()
        self.assertEqual(errors, 0)
        found: dict[str, set[str]] = {}
        for usr, _path, _line, _function, access, _peer in walker.refs:
            found.setdefault(walker.decls[usr]["name"], set()).add(access)
        return found

    def test_runtime_offsetof_takes_the_address_however_the_type_is_qualified(self):
        self.assertEqual(self.accesses(SOURCE), {
            "flat": {"address"},
            "bound": {"address"},
            "nested": {"address"},
            "pinned": {"layout"},
        })


HEADER = """\
#pragma once
struct Pair {
  int left;
  int right;
  friend bool operator==(const Pair&, const Pair&) = default;
};
struct Single {
  int value;
  bool operator==(const Single&) const = default;
};
"""
COMPARES = '#include "Pair.hpp"\nbool Same(const Pair& a, const Pair& b) { return a == b; }\n' \
    'bool Same(const Single& a, const Single& b) { return a == b; }\n'
DECLARES = '#include "Pair.hpp"\nint Size() { return sizeof(Pair) + sizeof(Single); }\n'


class DefaultedTests(LibclangTest):
    """A defaulted body exists only where it is used; only a production unit's use records its reads (#470)."""

    def reads(self, production: str, order: list[str]) -> set[str]:
        with tempfile.TemporaryDirectory() as root:
            include = os.path.join(root, "Game", "include")
            units = {"test": os.path.join(root, "Tests", "src", "UseTests.cpp"),
                     "production": os.path.join(root, "Game", "src", "Use.cpp")}
            for path, text in ((os.path.join(include, "Pair.hpp"), HEADER), (units["test"], COMPARES),
                               (units["production"], production)):
                os.makedirs(os.path.dirname(path), exist_ok=True)
                Path(path).write_text(text, encoding="utf-8")
            native_refs.relative.cache_clear()
            try:
                with mock.patch.object(native_refs, "NATIVE_PREFIX", os.path.normcase(root) + os.sep):
                    walker = native_refs.Walker()
                    index = native_refs.ci.Index.create()
                    for name in order:
                        errors = walker.unit(units[name], ["-x", "c++", "-std=c++20", f"-I{include}"], index)
                        self.assertEqual(errors, 0)
            finally:
                native_refs.relative.cache_clear()
        return {walker.decls[usr]["name"] for usr, _path, _line, _function, access, _peer in walker.refs
                if access == "read"}

    def test_a_comparison_used_only_by_tests_records_no_reads(self):
        for order in (["test", "production"], ["production", "test"]):
            with self.subTest(order=order):
                self.assertEqual(self.reads(DECLARES, order), set())

    def test_a_comparison_a_production_unit_uses_records_its_reads(self):
        for order in (["test", "production"], ["production", "test"]):
            with self.subTest(order=order):
                self.assertEqual(self.reads(COMPARES, order), {"left", "right", "value"})


REDECLARED = {
    "definition": ("Game\\src\\Data.cpp", "int shadowCount = 4;\n"),
    "production": ("Game\\src\\Pointers.cpp",
                   "extern int pendingCount;\nextern int shadowCount;\n"
                   "int Read() { return shadowCount + pendingCount; }\n"),
    "test": ("Tests\\src\\ShadowTests.cpp",
             "extern int shadowCount;\nextern int pendingCount;\n"
             "int Check() { return shadowCount + pendingCount; }\n"),
}


class RedeclarationTests(LibclangTest):
    """A redeclared variable's recorded location does not depend on unit or merge order (#472)."""

    def walk(self, groups: list[list[str]]) -> dict[str, tuple[str, int]]:
        """Walk each group of units in its own walker, in order, and merge the walkers in order."""
        with tempfile.TemporaryDirectory() as root:
            for path, text in REDECLARED.values():
                full = os.path.join(root, path)
                os.makedirs(os.path.dirname(full), exist_ok=True)
                Path(full).write_text(text, encoding="utf-8")
            native_refs.relative.cache_clear()
            decls: dict[str, dict] = {}
            ranks: dict[str, tuple] = {}
            try:
                with mock.patch.object(native_refs, "NATIVE_PREFIX", os.path.normcase(root) + os.sep):
                    index = native_refs.ci.Index.create()
                    for group in groups:
                        walker = native_refs.Walker()
                        for name in group:
                            path = os.path.join(root, REDECLARED[name][0])
                            self.assertEqual(walker.unit(path, ["-x", "c++", "-std=c++20"], index), 0)
                        native_refs.merge_decls(decls, ranks, walker.decls, walker.ranks)
            finally:
                native_refs.relative.cache_clear()
        return {entry["name"]: (entry["path"], entry["line"]) for entry in decls.values()}

    def test_the_definition_then_production_code_is_recorded_in_every_order(self):
        expected = {"shadowCount": ("Native\\Game\\src\\Data.cpp", 1),
                    "pendingCount": ("Native\\Game\\src\\Pointers.cpp", 1)}
        for order in itertools.permutations(REDECLARED):
            with self.subTest(walker=order):
                self.assertEqual(self.walk([list(order)]), expected)
            with self.subTest(merge=order):
                self.assertEqual(self.walk([[name] for name in order]), expected)


if __name__ == "__main__":
    unittest.main()
