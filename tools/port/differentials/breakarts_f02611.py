"""F02611 Menu::BreakArts::UpdateWeaponCategories title setup, MENU1.PRG 30.c:266."""

from __future__ import annotations

import random

from breakarts_differential import menu_item_stub
from differential import MODULE_BASES, Machine

MODULE_BASES["MENU/MENU1.PRG"] = 0x80102800

ID = "F02611"
ENTRY, END = 0x80102F68, 0x801031FC
MODULES = ("MENU/MENU1.PRG", "SLUS_010.40")
DRIVER = "breakarts_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BreakArtsCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS, SEED = 64, 2611
MUTANTS = {
    "initialize-condition-inverted": {0x80102F6C: 0x14800016},
}

VIEW_ARTS_NAME = 0x472
SET_MENU_ITEM = 0x800C8E5C
ITEM = 0x800F1000
CATEGORY_STEP = 0x80104510


def vector(rng: random.Random, number: int) -> list[int]:
    return [1]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    machine.map_zero(ITEM, 0x40)
    calls: list[int] = []

    machine.stub(
        SET_MENU_ITEM, menu_item_stub(calls, 0x80103478, 0, ITEM, 1)
    )
    machine.r[4] = vector[0]
    machine.execute(ENTRY)
    observation = [
        machine.r[2],
        machine.load(CATEGORY_STEP),
        1,
        *calls,
    ]
    return observation, [3, vector[0]]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return [] if native == observation else [
        f"category title native {native} original {observation}"
    ]
