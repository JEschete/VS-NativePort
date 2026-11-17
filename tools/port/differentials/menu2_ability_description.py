"""F02615: all 54 lookup entries and varied in-bounds imported string offsets."""

from __future__ import annotations

import random

import menu2_ability_foundation as h
from differential import Machine

globals().update(h.case_settings())
ID = "F02615"
ENTRY, END = 0x80103DD8, 0x80103E20
DRIVER = "menu2_ability_description_driver.cpp"
SEED = 2615
MUTANTS = {
    "lookup-base": {0x80103DDC: 0x24425079},
    "fallback-branch": {0x80103DEC: 0x14600009},
    "index-word-stride": {0x80103DF0: 0x00031800},
    "string-table-base": {0x80103DF8: 0x24424692},
    "offset-load-width": {0x80103E00: 0x90630000},
    "offset-word-stride": {0x80103E08: 0x00031800},
    "fallback-offset": {0x80103E1C: 0x2442505C},
}
NATIVE_REPLACE = ("menu_strings.subspan(fallback_offset)",
                  "menu_strings.subspan(fallback_offset + 1)")


def vector(rng: random.Random, number: int) -> list[int]:
    action = number % 54
    return [action, (number // 54 * 67 + action * 19) % 1060]


def run(machine: Machine, value: list[int]) -> tuple[dict, list[int]]:
    h.validate(machine)
    action, offset = value
    index = machine.load(h.DESCRIPTION_LOOKUP + action, 1)
    if index:
        machine.put(h.ABILITY_TEXT + index * 2, offset, 2)
    machine.r[4] = action
    machine.execute(ENTRY, limit=64)
    return h.observation(machine, [machine.r[2]], [], [], ["text pointer"]), [
        action, index, offset]


check = h.check
describe = h.describe
