"""F02610 Menu::BreakArts::UpdateArtList title setup, MENU1.PRG 30.c:104."""

from __future__ import annotations

import random

from breakarts_differential import menu_item_stub
from differential import MODULE_BASES, Machine

MODULE_BASES["MENU/MENU1.PRG"] = 0x80102800

ID = "F02610"
ENTRY, END = 0x80102A4C, 0x80102F68
MODULES = ("MENU/MENU1.PRG", "SLUS_010.40")
DRIVER = "breakarts_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BreakArtsCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS, SEED = 512, 2610
MUTANTS = {
    "force-cursor-memory-cleared": {0x80102A80: 0x00004021},
}

STRING_TABLE = 0x80103478
SET_MENU_ITEM = 0x800C8E5C
ITEM = 0x800F1000
STATE_STEP = 0x80104500
STATE_FORCE_CURSOR = 0x80104508
STATE_WEAPON_TYPE = 0x8010450C
STATE_IS_LAST_ROW = 0x8010452C
STATE_ANIMATION_INDEX = 0x8010452D


def vector(rng: random.Random, number: int) -> list[int]:
    categories = [1, 10, 4, 7, 2, 9, 3, 6, 8, 5]
    category = categories[number % len(categories)]
    high = [0, 1, 7, 15][number % 4] if number < 40 else rng.randrange(16)
    return [high * 16 + category, rng.randrange(0x800)]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    cursor_memory, menu_offset = vector
    category = cursor_memory & 0xF
    machine.put(STRING_TABLE + (category - 1) * 6, menu_offset, 2)
    machine.map_zero(ITEM, 0x40)
    calls: list[int] = []

    machine.stub(
        SET_MENU_ITEM, menu_item_stub(calls, STRING_TABLE, 0, ITEM)
    )
    machine.r[4] = cursor_memory
    machine.execute(ENTRY)
    calls[-1] = machine.load(ITEM + 9, 1)
    observation = [
        machine.r[2],
        machine.load(STATE_STEP),
        machine.load(STATE_WEAPON_TYPE),
        machine.load(STATE_FORCE_CURSOR),
        machine.load(STATE_IS_LAST_ROW, 1),
        machine.load(STATE_ANIMATION_INDEX, 1),
        1,
        *calls,
    ]
    return observation, [2, cursor_memory, menu_offset]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return [] if native == observation else [
        f"title setup native {native} original {observation}"
    ]
