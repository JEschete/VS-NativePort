"""F02619: learned-row pagination, exact fixed copies and repeat-entry no-op."""

from __future__ import annotations

import random

import menu2_ability_foundation as h
from differential import Machine

globals().update(h.case_settings())
ID = "F02619"
ENTRY, END = 0x80102928, 0x80102B5C
DRIVER = "menu2_ability_rows_driver.cpp"
SEED = 2619
MUTANTS = {
    "existing-rows-gate": {0x8010294C: 0x14400003},
    "cursor-enabled-gate": {0x80102968: 0x14600009},
    "cursor-slot-stride": {0x80102978: 0x00051800},
    "visible-capacity": {0x80102990: 0x2603FFF9},
    "short-list-gate": {0x80102994: 0x04600007},
    "short-list-merge": {0x8010299C: 0x01004021},
    "short-list-reset": {0x801029A4: 0x1040001A},
    "overflow-reset": {0x801029C0: 0x10400004},
    "top-arrow-gate": {0x801029E4: 0x11000004},
    "top-arrow-cursor": {0x801029EC: 0x24080002},
    "top-arrow-scroll": {0x801029F0: 0x24E7FFFE},
    "bottom-arrow-row": {0x801029FC: 0x24020006},
    "bottom-arrow-cursor": {0x80102A08: 0x24080005},
    "bottom-arrow-scroll": {0x80102A0C: 0x24E70002},
    "risk-kind": {0x80102A20: 0x24020019},
    "risk-action": {0x80102A44: 0x90440001},
    "allocation-stride": {0x80102A50: 0x00102180},
    "count-store": {0x80102A58: 0xA0404EC1},
    "menu-state-reset": {0x80102A64: 0},
    "null-name-gate": {0x80102A94: 0x14600010},
    "name-spacing": {0x80102A98: 0x240210FB},
    "name-copy-length": {0x80102AC4: 0x28E2000D},
    "name-terminator-offset": {0x80102AD4: 0xA4AC001C},
    "null-name-terminator": {0x80102AD8: 0xA4A00000},
    "null-description-gate": {0x80102AE4: 0x14600010},
    "description-chunk": {0x80102AE8: 0x240200F9},
    "description-copy-length": {0x80102B14: 0x28E2002D},
    "description-terminator-offset": {0x80102B24: 0xA4AC007C},
    "null-description-terminator": {0x80102B28: 0xA4A00020},
    "destination-row-stride": {0x80102B2C: 0x24A5007E},
    "source-pointer-stride": {0x80102B40: 0x24C60004},
    "completion-result": {0x80102B44: 0x24020000},
}
NATIVE_REPLACE = ("constexpr std::uint16_t kNameSpacing = 0x10FA;",
                  "constexpr std::uint16_t kNameSpacing = 0x10FB;")
TEXT_POINTER = 0x80104EB4
HIGHLIGHTED = 0x80104EB8
MENU_STATE = 0x80104EBC
TYPE = 0x80104EC0
COUNT = 0x80104EC1
FIRST = 0x80104EC2
TEXT_INPUT = 0x80200000
STRINGS = 0x80210000
ALLOCATION = 0x80220000
TEXT_WORDS = 16 * 64 + 1


def vector(rng: random.Random, number: int) -> list[int]:
    count = number % 16 + 1
    kind = 24 + (number // 16) % 2
    # Independent paging axes deliberately reach both scroll-arrow adjustments.
    highlighted = (number // 32) % 8
    first = rng.choice((0, 1, 2, max(0, count - 8), count, 255))
    if number % 37 == 0:
        highlighted = 128 + (number // 37) % 128
        first = 255
    remembered = 0 if number % 7 == 0 else 1
    return [count, kind, remembered, highlighted, first, int(number % 17 == 0),
            rng.getrandbits(16), rng.getrandbits(16), rng.getrandbits(16)]


def run(machine: Machine, value: list[int]) -> tuple[dict, list[int]]:
    count, kind, remembered, highlighted, first, existing, null_names, null_descriptions, seed = value
    h.validate(machine)
    machine.put(TEXT_POINTER, ALLOCATION if existing else 0)
    machine.put(HIGHLIGHTED, 3)
    machine.put(MENU_STATE, 0x12345678)
    machine.put(TYPE, 25, 1)
    machine.put(COUNT, 6, 1)
    machine.put(FIRST, 2, 1)
    machine.map_zero(h.MENU_MEMORY, 64)
    machine.put(h.MENU_MEMORY + kind * 2, highlighted, 1)
    machine.put(h.MENU_MEMORY + kind * 2 + 1, first, 1)
    machine.put(h.CURSOR, remembered, 1)
    for index in range(16):
        machine.put(0x801050D0 + index, 24 + index, 1)
        machine.put(0x801050E0 + index, 40 + index, 1)
    machine.map_zero(TEXT_INPUT, 16 * 8)
    for row in range(count):
        name = STRINGS + row * 0x100
        description = name + 0x40
        machine.put(TEXT_INPUT + row * 8, 0 if null_names & (1 << row) else name)
        machine.put(TEXT_INPUT + row * 8 + 4,
                    0 if null_descriptions & (1 << row) else description)
        for index in range(14):
            machine.put(name + index * 2, seed ^ (row * 131 + index * 19), 2)
        for index in range(46):
            machine.put(description + index * 2,
                        seed ^ (row * 313 + index * 23 + 0x1000), 2)
    for index in range(TEXT_WORDS):
        machine.put(ALLOCATION + index * 2, seed + index * 31, 2)
    calls = []

    def allocate(m: Machine) -> None:
        calls.append([12, m.r[4]])
        m.r[2] = ALLOCATION

    machine.stub(0x80102864, lambda m: calls.append([11, m.r[4]]))
    machine.stub(0x80043E3C, allocate)
    machine.r[4:7] = [count, kind, TEXT_INPUT]
    machine.execute(ENTRY, limit=20000)
    values = [
        machine.r[2], machine.load(COUNT, 1), machine.load(MENU_STATE),
        machine.load(TYPE, 1), machine.load(HIGHLIGHTED), machine.load(FIRST, 1),
        int(machine.load(TEXT_POINTER) != 0),
        *(machine.load(ALLOCATION + index * 2, 2) for index in range(TEXT_WORDS)),
    ]
    allowed = [(TEXT_POINTER, TEXT_POINTER + 4), (HIGHLIGHTED, HIGHLIGHTED + 4),
               (MENU_STATE, MENU_STATE + 4), (TYPE, FIRST + 1),
               (ALLOCATION, ALLOCATION + count * 128)]
    names = ["result", "count", "menu state", "type", "highlighted", "first", "allocated"]
    return h.observation(machine, values, calls, allowed, names), value


check = h.check
describe = h.describe
