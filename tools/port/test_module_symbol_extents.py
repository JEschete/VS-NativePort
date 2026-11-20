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
MODULE_DATA = REPO / "Native" / "Game" / "src" / "Boot" / "ModuleData.cpp"
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
ADDRESS = re.compile(r"0x(?P<value>[0-9A-F]+)u")
SIZE = re.compile(r"(?P<value>\d+)u")
TARGET = re.compile(
    r"(?:\(void\*\)|(?:static|reinterpret)_cast<void\*>\(|const_cast<void\*>\(static_cast<const (?:volatile )?void\*>\()?"
    r"&?(?P<name>[\w:]+)(?:\.data\(\))?\)*"
)


def normalized(field: str) -> str:
    return re.sub(r"\s*([()<>*&])\s*", r"\1", " ".join(field.split()))


def initializer_fields(table: str) -> list[list[str]]:
    """Split the body of a braced table into each element's top-level fields, whatever the line layout."""
    entries: list[list[str]] = []
    fields: list[str] = []
    field = ""
    depth = 0
    for ch in COMMENT.sub("", table):
        if ch in "({":
            depth += 1
            if depth == 1:
                fields, field = [], ""
                continue
        elif ch in ")}":
            depth -= 1
            if depth == 0:
                fields.append(normalized(field))
                entries.append(fields)
                continue
        elif ch == "," and depth == 1:
            fields.append(normalized(field))
            field = ""
            continue
        if depth >= 1:
            field += ch
    return entries


def symbol_entries(table: str) -> list[tuple[str, int, int, str, bool]]:
    """Each ModuleSymbol initializer as (module, address, size, bound name, has interior extent)."""
    symbols = []
    for fields in initializer_fields(table):
        if len(fields) != 7 or not fields[0].startswith('"'):
            continue
        module, address, size, target, _native, source_element, native_element = fields
        address_match, size_match, target_match = (
            ADDRESS.fullmatch(address), SIZE.fullmatch(size), TARGET.fullmatch(target))
        if not address_match or not size_match or not target_match:
            continue
        interior = source_element != "0u" and native_element != "0u"
        symbols.append((module.strip('"'), int(address_match["value"], 16), int(size_match["value"]),
                        target_match["name"], interior))
    return symbols


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
        start = text.index("{", text.index("const VS::Content::ModuleSymbol VS::Content::kModuleSymbols[] = {")) + 1
        by_module: dict[str, list[tuple[int, int, str, bool]]] = {}
        for module, address, size, name, interior in symbol_entries(text[start:text.index("\n};", start)]):
            by_module.setdefault(module, []).append((address, size, name, interior))
        return by_module

    def test_wrapped_and_cast_entries_are_parsed(self):
        # Promoted functions bind their module address to a qualified Live entry (#397); clang-format wraps entries
        # and const tables bind through const_cast (#483).
        table = (
            '    {"BATTLE/BATTLE.PRG", 0x8006CDB8u, 0u,\n'
            "     reinterpret_cast<void*>(&VS::Effects::LiveSignalActorEffectAnimation), 0u, 0u, 0u},\n"
            '    {"BATTLE/BATTLE.PRG", 0x80068BECu, 14u,\n'
            "     const_cast<void*>(\n"
            "         static_cast<const void*>(&VS::Field::kPlatformBounceOffsets)),\n"
            "     0u, 0u, 0u},\n"
            '    {"SLUS_010.40", 0x80010000u, 335872u, (void*)Content_residentLfoSamples, sizeof(decltype(x)), 2u,\n'
            "     2u},\n"
            '    {"BATTLE/BATTLE.PRG", 0x80069998u, 9u, &VS::Battle::puzzleTimerText,\n'
            "     sizeof VS::Battle::puzzleTimerText, 1u, 1u},\n"
            '    {"SLUS_010.40", 0x8004B9DCu, 13312u, VS::Battle::actionDefinitions.data(),\n'
            "     sizeof VS::Battle::actionDefinitions, 1u, 1u},\n"
            '    {"SLUS_010.40", 0x800377F0u, 4u,\n'
            "     const_cast<void*>(static_cast<const volatile void*>(\n"
            "         &VS::Audio::Cutscene::legacySpuTransferBusy)),\n"
            "     sizeof VS::Audio::Cutscene::legacySpuTransferBusy, 1u, 1u},\n"
        )
        self.assertEqual(symbol_entries(table), [
            ("BATTLE/BATTLE.PRG", 0x8006CDB8, 0, "VS::Effects::LiveSignalActorEffectAnimation", False),
            ("BATTLE/BATTLE.PRG", 0x80068BEC, 14, "VS::Field::kPlatformBounceOffsets", False),
            ("SLUS_010.40", 0x80010000, 335872, "Content_residentLfoSamples", True),
            ("BATTLE/BATTLE.PRG", 0x80069998, 9, "VS::Battle::puzzleTimerText", True),
            ("SLUS_010.40", 0x8004B9DC, 13312, "VS::Battle::actionDefinitions", True),
            ("SLUS_010.40", 0x800377F0, 4, "VS::Audio::Cutscene::legacySpuTransferBusy", True),
        ])

    def test_interior_extents_end_before_the_next_entry(self):
        # The reviewed blobs keep the retired copier's Content_ names; their native storage is VS::Content (#483).
        blobs = {"VS::Content::" + blob[4].removeprefix("Content_") for blob in copy_fixups.MODULE_DATA_BLOBS}
        module_data = MODULE_DATA.read_text(encoding="utf-8")
        self.assertEqual([name for name in sorted(blobs) if not re.search(re.escape(name) + r"\s*\[", module_data)], [])
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
