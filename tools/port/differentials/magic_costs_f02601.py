"""F02601 Menu::Magic::SetSpellMpCost, MENU0.PRG _setMPCost 84.c:18."""

from __future__ import annotations

import random

from differential import Machine
from magic_costs_common import (
    ACTION_COST_OFFSET,
    ACTION_DEFINITION_SIZE,
    ACTION_DEFINITIONS,
    COMPILER,
    COMPILER_OPTIONS,
    CXX_STANDARD,
    DRIVER,
    GET_ACTION_FLAGS,
    INCLUDES,
    MODULES,
    SOURCES,
    compare,
    flatten,
    install_cost_text,
)

__all__ = ("COMPILER", "COMPILER_OPTIONS", "CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES",
           "SOURCES")

ID = "F02601"
ENTRY, END = 0x80102884, 0x801029B8
VECTORS, SEED = 512, 2601
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "cost-offset": {0x80102904: 0x90440002},
    "variable-bracket": {0x8010292C: 0x2413005C},
    "variable-x-shift": {0x80102980: 0x001230C0},
    "value-disabled-mask": {0x8010298C: 0x30E70003},
    "variable-ignored": {0x801028AC: 0x24120000},
}


def vector(rng: random.Random, number: int) -> list[int]:
    edge_costs = [0, 1, 9, 10, 99, 100, 255]
    cost = edge_costs[number % len(edge_costs)] if number < 64 else rng.randrange(256)
    action_id = 0 if number % 37 == 0 else rng.randrange(256)
    sign = -1 if number & 1 else 1
    return [sign * action_id, cost, rng.getrandbits(16)]


def run(machine: Machine, vector: list[int]) -> tuple[list[list[int]], list[int]]:
    signed_id, cost, flags = vector
    action_id = abs(signed_id)
    machine.put(ACTION_DEFINITIONS + action_id * ACTION_DEFINITION_SIZE + ACTION_COST_OFFSET,
                cost, 1)
    events: list[list[int]] = []

    def get_flags(m: Machine) -> None:
        if m.r[4] != 0 or m.r[5] != action_id:
            raise ValueError(f"unexpected action-flags arguments {m.r[4:6]}")
        m.r[2] = flags

    machine.stub(GET_ACTION_FLAGS, get_flags)
    install_cost_text(machine, events)
    machine.r[4] = signed_id & 0xFFFFFFFF
    machine.execute(ENTRY)
    return events, [1, *vector]


def check(image, vector: list[int], observation: list[list[int]], native: list[int],
          reference: list[list[int]]) -> list[str]:
    return compare(native, [0, *flatten(observation)])
