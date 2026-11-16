"""F02603 Menu::Magic::TeleportMpCost, MENU0.PRG _getTeleportCost 84.c:117."""

from __future__ import annotations

import random

from differential import Machine
from magic_costs_common import (
    COMPILER,
    COMPILER_OPTIONS,
    CXX_STANDARD,
    DRIVER,
    GET_CURRENT_ROOM_ID,
    INCLUDES,
    MODULES,
    SAVE_POINT_COUNT,
    SAVE_POINT_DATA,
    SOURCES,
    compare,
)

__all__ = ("COMPILER", "COMPILER_OPTIONS", "CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES",
           "SOURCES")

ID = "F02603"
ENTRY, END = 0x80102A68, 0x80102B08
VECTORS, SEED = 512, 2603
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "room-stride": {0x80102AA8: 0x24630008},
    "room-sign-extended": {0x80102A8C: 0x84620000},
    "save-point-limit": {0x80102AA0: 0x2882002E},
    "interval-order-ignored": {0x80102AB0: 0x10000003},
    "base-cost": {0x80102ACC: 0x2405000E},
    "interval-cost-ignored": {0x80102AE4: 0x00A02821},
}


def vector(rng: random.Random, number: int) -> list[int]:
    """[target, room id, custom table, 48 save-point words (custom tables only)]."""
    target = number % (SAVE_POINT_COUNT + 1) if number < 2 * (SAVE_POINT_COUNT + 1) \
        else rng.randrange(SAVE_POINT_COUNT + 1)
    custom = number & 1
    words = []
    if custom:
        rooms = [rng.randrange(1, 8) for _ in range(4)]
        words = [rng.choice(rooms) | rng.getrandbits(16) << 16 if rng.randrange(4) else
                 rng.getrandbits(32) for _ in range(SAVE_POINT_COUNT)]
        candidates = [word & 0xFFFF for word in words] + rooms
        if number & 2:
            # A room only the last save points carry, beyond the halfword sign bit.
            late = 0x8000 | rng.getrandbits(15)
            words[rng.randrange(SAVE_POINT_COUNT - 4, SAVE_POINT_COUNT)] = \
                late | rng.getrandbits(16) << 16
            candidates = [late]
    else:
        candidates = [0x1B, 0x0E, 0x199, 0xD2, 0x8B, 0]
    room = rng.choice(candidates) if rng.randrange(8) else rng.randrange(0x10000)
    return [target, room, custom, *words]


def run(machine: Machine, vector: list[int]) -> tuple[int, list[int]]:
    target, room, custom, *words = vector
    if custom:
        for index, word in enumerate(words):
            machine.put(SAVE_POINT_DATA + index * 4, word)
    else:
        words = [machine.load(SAVE_POINT_DATA + index * 4) for index in range(SAVE_POINT_COUNT)]

    def current_room(m: Machine) -> None:
        m.r[2] = room

    machine.stub(GET_CURRENT_ROOM_ID, current_room)
    machine.r[4] = target
    machine.execute(ENTRY)
    cost = machine.r[2]
    return cost - (1 << 32) if cost >> 31 else cost, [3, target, room, *words]


def check(image, vector: list[int], observation: int, native: list[int],
          reference: int) -> list[str]:
    return compare(native, [1, observation])
