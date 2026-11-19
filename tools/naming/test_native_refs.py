"""Tests for the libclang reference walker in native_refs.py."""
from __future__ import annotations

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


class OffsetofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            native_refs.configure(native_refs.libclang_path())
        except Exception as error:  # noqa: BLE001 - any failure to load libclang means the walker cannot run
            raise unittest.SkipTest(f"libclang is unavailable: {error}") from error

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


if __name__ == "__main__":
    unittest.main()
