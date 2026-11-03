"""Module symbol extents end at the next source symbol (#280).

The loader resolves an address without an exact entry through the nearest preceding entry's interior. An extent that
runs past the next source symbol would bind a retired, unemitted or unnamed label into a neighbour's storage.
"""

import re
import types
import unittest
from pathlib import Path

import copy_fixups
import copy_to_native

REPO = Path(__file__).resolve().parents[2]
MODULE_POINTERS = REPO / "Native" / "Game" / "src" / "Boot" / "ModulePointers.cpp"
ENTRY = re.compile(
    r'^\s*\{"(?P<module>[^"]+)", 0x(?P<address>[0-9A-F]+)u, (?P<size>\d+)u, \(void\*\)&?(?P<name>\w+), '
    r"(?P<native>[^,]+), (?P<source_element>[^,]+), (?P<native_element>[^}]+)\},"
)


def copier(views: dict[str, tuple[str, int]]) -> types.SimpleNamespace:
    fake = types.SimpleNamespace(views=views)
    fake.view_root = lambda data_id: copy_to_native.Copier.view_root(fake, data_id)
    return fake


def boundaries(splat: list[tuple[str, int, str]], data: dict[str, str], views: dict[str, tuple[str, int]],
               module: str = "MOD"):
    rows = [{"name": name, "vram_start": f"{address:08X}", "type": kind} for name, address, kind in splat]
    data_by_key = {(module, name): {"id": data_id} for name, data_id in data.items()}
    return copy_to_native.Copier.source_boundaries(copier(views), module, rows, data_by_key)


class SourceBoundaryTests(unittest.TestCase):
    def test_an_unemitted_label_ends_the_preceding_extent(self):
        limit = boundaries([("table", 0x100, "data"), ("D_110", 0x110, "data")], {"table": "G1"}, {})
        self.assertEqual(limit(0x100, "G1"), 0x110)

    def test_a_retired_global_ends_the_preceding_extent(self):
        limit = boundaries([("table", 0x100, "data"), ("state", 0x110, "data")], {"table": "G1", "state": "G2"}, {})
        self.assertEqual(limit(0x100, "G1"), 0x110)

    def test_a_view_of_the_same_global_does_not_end_it(self):
        splat = [("table", 0x100, "data"), ("middle", 0x108, "data"), ("tail", 0x10C, "data"), ("next", 0x120, "data")]
        data = {"table": "G1", "middle": "G2", "tail": "G3", "next": "G4"}
        limit = boundaries(splat, data, {"G2": ("G1", 8), "G3": ("G2", 4)})
        self.assertEqual(limit(0x100, "G1"), 0x120)

    def test_a_view_of_another_global_ends_it(self):
        splat = [("first", 0x100, "data"), ("second", 0x110, "data"), ("view", 0x118, "data")]
        limit = boundaries(splat, {"first": "G1", "second": "G2", "view": "G3"}, {"G3": ("G2", 8)})
        self.assertEqual(limit(0x100, "G1"), 0x110)
        self.assertIsNone(limit(0x110, "G2"))

    def test_a_function_ends_the_preceding_extent(self):
        limit = boundaries([("table", 0x100, "data"), ("func_104", 0x104, "func")], {"table": "G1"}, {})
        self.assertEqual(limit(0x100, "G1"), 0x104)

    def test_an_alias_at_the_same_address_does_not_end_it(self):
        splat = [("table", 0x100, "data"), ("alias", 0x100, "data"), ("next", 0x140, "data")]
        limit = boundaries(splat, {"table": "G1"}, {})
        self.assertEqual(limit(0x100, "G1"), 0x140)

    def test_the_last_symbol_has_no_limit(self):
        limit = boundaries([("table", 0x100, "data")], {"table": "G1"}, {})
        self.assertIsNone(limit(0x100, "G1"))

    def test_a_reviewed_source_blob_ends_the_preceding_extent(self):
        # vs_battle_mapCompletionFlags[16] ends where the unnamed trigger marker bytes begin (146C.c:17377).
        splat = [("vs_battle_mapCompletionFlags", 0x800E8508, "data"), ("D_800E856C", 0x800E856C, "data")]
        limit = boundaries(splat, {"vs_battle_mapCompletionFlags": "G00038"}, {}, "BATTLE/BATTLE.PRG")
        self.assertEqual(limit(0x800E8508, "G00038"), 0x800E8548)


class GeneratedSymbolTableTests(unittest.TestCase):
    def entries(self) -> dict[str, list[tuple[int, int, str, bool]]]:
        text = MODULE_POINTERS.read_text(encoding="utf-8")
        start = text.index("const Content_ModuleSymbol Content_moduleSymbols[] = {")
        body = text[start:text.index("\n};", start)]
        by_module: dict[str, list[tuple[int, int, str, bool]]] = {}
        for line in body.splitlines():
            m = ENTRY.match(line)
            if m:
                interior = m["source_element"].strip() != "0u" and m["native_element"].strip() != "0u"
                by_module.setdefault(m["module"], []).append(
                    (int(m["address"], 16), int(m["size"]), m["name"], interior))
        return by_module

    def test_interior_extents_end_before_the_next_entry(self):
        blobs = {blob[4] for blob in copy_fixups.MODULE_DATA_BLOBS}
        by_module = self.entries()
        self.assertGreater(sum(map(len, by_module.values())), 1000)
        crossing = []
        for module, entries in by_module.items():
            starts = sorted({address for address, _, _, _ in entries})
            for address, size, name, interior in entries:
                if not interior or name in blobs:
                    continue
                later = [start for start in starts if start > address]
                if later and address + size > later[0]:
                    crossing.append(f"{module} {name} 0x{address:08X}+{size} crosses 0x{later[0]:08X}")
        self.assertEqual(crossing, [])


if __name__ == "__main__":
    unittest.main()
