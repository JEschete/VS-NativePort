"""F02599 Menu::Magic::CreateHeader, MENU0.PRG _setMenuHeader 15AC.c:148."""

from __future__ import annotations

import random

from differential import Machine
from magic_costs_common import (
    COMPILER,
    COMPILER_OPTIONS,
    CXX_STANDARD,
    DRIVER,
    EVENT_INITIALIZE_INFORMATION_BOX,
    EVENT_SET_MENU_COMMAND,
    EVENT_SET_MENU_ITEM,
    INCLUDES,
    INITIALIZE_INFORMATION_BOX,
    MENU_ITEM_ROW,
    MENU_STRINGS,
    MODULES,
    SET_MENU_COMMAND,
    SET_MENU_ITEM,
    SOURCES,
    compare,
    flatten,
    recorder,
    signed,
)

__all__ = ("COMPILER", "COMPILER_OPTIONS", "CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES",
           "SOURCES")

ID = "F02599"
ENTRY, END = 0x80104254, 0x801042C8
VECTORS, SEED = 64, 2599
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "header-string-stride": {0x80104280: 0x00021080},
    "header-target-position": {0x8010429C: 0x240300B5},
    "header-selected": {0x801042A4: 0x24030002},
    "header-command": {0x80104290: 0x24040003},
    "header-width": {0x80104278: 0x2407008D},
}
ROW_SIZE = 0x40


def vector(rng: random.Random, number: int) -> list[int]:
    """[label offset]: the halfword offset the battle menu strings hold for the header."""
    return [(1, 0x20, 0x45, 0x100)[number] if number < 4 else rng.randrange(1, 0x800)]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    (offset,) = vector
    machine.put(MENU_STRINGS, offset, 2)
    machine.map_zero(MENU_ITEM_ROW, ROW_SIZE)
    events: list[list[int]] = []

    def set_menu_item(m: Machine) -> None:
        text = m.load(m.r[29] + 0x14)
        if (text - MENU_STRINGS) & 1:
            raise ValueError(f"header text pointer {text:#x} is misaligned")
        events.append([EVENT_SET_MENU_ITEM, signed(m.r[4]), signed(m.r[5]), signed(m.r[6]),
                       signed(m.r[7]), m.load(m.r[29] + 0x10, 4, True),
                       (text - MENU_STRINGS) // 2])
        m.r[2] = MENU_ITEM_ROW

    machine.stub(SET_MENU_ITEM, set_menu_item)
    machine.stub(SET_MENU_COMMAND, recorder(events, EVENT_SET_MENU_COMMAND, 1))
    machine.stub(INITIALIZE_INFORMATION_BOX, recorder(events, EVENT_INITIALIZE_INFORMATION_BOX, 0))
    machine.execute(ENTRY)
    observation = [
        machine.load(MENU_ITEM_ROW, 1),
        machine.load(MENU_ITEM_ROW + 0x18, 2, True),
        machine.load(MENU_ITEM_ROW + 6, 1),
        *flatten(events),
    ]
    return observation, [5, offset, offset + 1]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return compare(native, [0, *observation])
