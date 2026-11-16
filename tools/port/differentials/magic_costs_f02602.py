"""F02602 Menu::Magic::SetTeleportMpCost, MENU0.PRG _setMPCostDirect 84.c:64."""

from __future__ import annotations

import random

from differential import Machine
from magic_costs_common import (
    COMPILER,
    COMPILER_OPTIONS,
    CXX_STANDARD,
    DRIVER,
    INCLUDES,
    MODULES,
    SOURCES,
    compare,
    flatten,
    install_cost_text,
)

__all__ = ("COMPILER", "COMPILER_OPTIONS", "CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES",
           "SOURCES")

ID = "F02602"
ENTRY, END = 0x801029B8, 0x80102A68
VECTORS, SEED = 512, 2602
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "label-enabled": {0x801029DC: 0x00003821},
    "digit-base": {0x80102A18: 0x24420031},
    "single-digit": {0x80102A20: 0x00000000},
    "value-x": {0x80102A40: 0x24060049},
    "value-prefix": {0x80102A38: 0x24020024},
}


def vector(rng: random.Random, number: int) -> list[int]:
    # The menu stores teleport costs as halfwords; 0xFFFF is the largest formatted cost.
    edge_costs = [0, 1, 9, 10, 15, 99, 100, 999, 1000, 9999, 10000, 65535]
    if number < 2 * len(edge_costs):
        cost = edge_costs[number % len(edge_costs)]
    else:
        cost = rng.choice((rng.randrange(256), rng.randrange(65536)))
    return [cost, number // len(edge_costs) & 1 if number < 24 else rng.randrange(2)]


def run(machine: Machine, vector: list[int]) -> tuple[list[list[int]], list[int]]:
    cost, disabled = vector
    events: list[list[int]] = []
    install_cost_text(machine, events)
    machine.r[4] = cost
    machine.r[5] = disabled
    machine.execute(ENTRY)
    return events, [2, *vector]


def check(image, vector: list[int], observation: list[list[int]], native: list[int],
          reference: list[list[int]]) -> list[str]:
    return compare(native, [0, *flatten(observation)])
