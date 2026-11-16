"""Shared original-instruction and native-driver support for the MENU0 MP cost helpers."""

from __future__ import annotations

from collections.abc import Callable

import differential
from differential import Machine
from native_compile_support import COMPILER, COMPILER_OPTIONS, CXX_STANDARD

MODULE = "MENU/MENU0.PRG"
MODULE_BASE = 0x80102800
MODULES = (MODULE,)
DRIVER = "magic_costs_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\MagicCosts.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
INCLUDES = (
    r"Native\Engine\include",
    r"Native\Game\include",
)

differential.MODULE_BASES[MODULE] = MODULE_BASE

__all__ = ("COMPILER", "COMPILER_OPTIONS", "CXX_STANDARD")

# Original callees.
GET_ACTION_FLAGS = 0x8008A5D0
TO_BCD = 0x800CCC54
SET_ACTION_COST = 0x80100100
GET_CURRENT_ROOM_ID = 0x8007C874
FLY_OUT = 0x800FA92C
ROWS_IDLE = 0x800FA9D0
SET_MENU_ROWS = 0x801005E0
SELECTED_ROW = 0x801008C8
HIGHLIGHTED_ROW = 0x801008B0
CLEAR_MENU_EXCEPT = 0x800FA8E0
DISMISS_INFORMATION_BOX = 0x800FFBA8
SET_MENU_COMMAND = 0x800FFA88
INITIALIZE_INFORMATION_BOX = 0x800FFBC8
SET_MENU_ITEM = 0x800C8E5C

# Original functions of this group the teleport menu calls directly.
SET_TELEPORT_MP_COST = 0x801029B8
TELEPORT_MP_COST = 0x80102A68

# Original data.
ACTION_DEFINITIONS = 0x8004B9DC
ACTION_DEFINITION_SIZE = 52
ACTION_COST_OFFSET = 3
SAVE_POINT_DATA = 0x801067DC
SAVE_POINT_COUNT = 48
TELEPORTATION_STRINGS = 0x80105D94
LOCATIONS_INDEX = 3
DISABLED_DESCRIPTION_OFFSET = 144
SAVE_POINT_STATES = 0x80061598 + 0x156
CHARACTER_POINTER = 0x800F19FC
ACTION_RESTRICTIONS = 0x800F4EA0
CURSOR_MEMORIES = 0x800F4EE8
TELEPORT_CURSOR_ROW = CURSOR_MEMORIES + 0x16
TELEPORT_CURSOR_SCROLL = CURSOR_MEMORIES + 0x17
CURSOR_MEMORY_SETTING = 0x80060021
DISPLAY_ACTION_COST = 0x801022D4
MENU_STRINGS = 0x800EB9DC

# Teleport menu statics.
TELEPORT_STEP = 0x80106A10
TELEPORT_CURRENT_MP = 0x80106A0C
TELEPORT_SELECTED_ROW = 0x80106A08
TELEPORT_SAVE_POINTS = 0x80106978
TELEPORT_COSTS = 0x801069A8

# Scratch memory for the character and the setMenuItem row.
CHARACTER = 0x80300000
CHARACTER_STATE = 0x80300100
MENU_ITEM_ROW = 0x80310000

# Recorded call codes, shared with magic_costs_driver.cpp.
EVENT_FLY_OUT = 1
EVENT_SET_MENU_ROWS = 2
EVENT_CLEAR_MENU_EXCEPT = 3
EVENT_DISMISS_INFORMATION_BOX = 4
EVENT_SET_MENU_COMMAND = 5
EVENT_SET_ACTION_COST = 6
EVENT_INITIALIZE_INFORMATION_BOX = 7
EVENT_SET_MENU_ITEM = 8


def signed(value: int, bits: int = 32) -> int:
    value &= (1 << bits) - 1
    return value - (1 << bits) if value >> (bits - 1) else value


def read_text(machine: Machine, address: int) -> list[int]:
    text = []
    for offset in range(32):
        char = machine.load(address + offset, 1)
        if char == 0:
            return text
        text.append(char)
    raise ValueError("action-cost text is unterminated")


def install_cost_text(machine: Machine, events: list[list[int]]) -> None:
    """Stub toBCD and record each setActionCost(index, text, x, disabled)."""

    def to_bcd(m: Machine) -> None:
        value = m.r[4] & 0xFFFFFFFF
        m.r[2] = (value % 10 | (value // 10) * 16) & 0xFFFFFFFF

    def set_action_cost(m: Machine) -> None:
        text = read_text(m, m.r[5])
        events.append([EVENT_SET_ACTION_COST, signed(m.r[4]), signed(m.r[6]), signed(m.r[7]),
                       len(text), *text])

    machine.stub(TO_BCD, to_bcd)
    machine.stub(SET_ACTION_COST, set_action_cost)


def recorder(events: list[list[int]], code: int, arguments: int) -> Callable[[Machine], None]:
    def record(m: Machine) -> None:
        events.append([code, *(signed(m.r[4 + index]) for index in range(arguments))])

    return record


def flatten(events: list[list[int]]) -> list[int]:
    values = [len(events)]
    for event in events:
        values.extend((len(event), *event))
    return values


def compare(native: list[int], expected: list[int]) -> list[str]:
    if native == expected:
        return []
    for index, (left, right) in enumerate(zip(native, expected, strict=False)):
        if left != right:
            return [f"output {index}: native {left} original {right} "
                    f"(native {len(native)} values, original {len(expected)})"]
    return [f"native {len(native)} values, original {len(expected)}"]
