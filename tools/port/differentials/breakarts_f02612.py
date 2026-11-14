"""F02612 Menu::BreakArts::CreateHeader, MENU1.PRG 30.c:344."""

from __future__ import annotations

import random

from breakarts_differential import menu_item_stub
from differential import MODULE_BASES, Machine

MODULE_BASES["MENU/MENU1.PRG"] = 0x80102800

ID = "F02612"
ENTRY, END = 0x801031FC, 0x80103270
MODULES = ("MENU/MENU1.PRG", "SLUS_010.40")
DRIVER = "breakarts_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BreakArtsCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS, SEED = 64, 2612
MUTANTS = {
    "header-x-shifted": {0x8010322C: 0x2405013F},
}

BATTLE_MENU_STRINGS = 0x800EB9DC
SET_MENU_ITEM = 0x800C8E5C
SET_COMMAND = 0x800FFA88
INITIALIZE_INFORMATION = 0x800FFBC8
ITEM = 0x800F1000


def vector(rng: random.Random, number: int) -> list[int]:
    return [number % 32]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    header_index = vector[0]
    machine.map_zero(BATTLE_MENU_STRINGS, 0x100)
    machine.put(BATTLE_MENU_STRINGS + 2, header_index, 2)
    machine.map_zero(ITEM, 0x40)
    calls: list[int] = []

    machine.stub(
        SET_MENU_ITEM,
        menu_item_stub(calls, BATTLE_MENU_STRINGS, 2, ITEM),
    )
    machine.stub(
        SET_COMMAND, lambda m: calls.extend([2, 1, m.r[4]])
    )
    machine.stub(INITIALIZE_INFORMATION, lambda m: calls.extend([3, 0]))
    machine.execute(ENTRY)
    observation = [3, *calls]
    return observation, [4, header_index]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return [] if native == observation else [
        f"header setup native {native} original {observation}"
    ]
