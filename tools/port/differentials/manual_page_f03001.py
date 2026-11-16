"""F03001 Menu::Manual::DrawPageBackground, MENUE.PRG _setPageBg 494.c:716."""

from __future__ import annotations

import random

from differential import Machine
from manual_page_common import (
    DRAWN,
    DRIVER,
    INCLUDES,
    MODULES,
    OP_BACKGROUND,
    REFUSED,
    SOURCES,
    check,
    counts,
    events,
    install_frame,
    int16,
    int32,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "check")

ID = "F03001"
ENTRY, END = 0x80104204, 0x8010435C
VECTORS, SEED = 256, 3001
MUTANT_VECTORS = 128
MUTANTS = {
    "packet-length": {0x80104210: 0x24020009},
    "semi-transparent": {0x80104234: 0x2402003A},
    "right-edge-inclusive": {0x80104254: 0x24C60000},
    "bottom-edge-inclusive": {0x80104280: 0x24A50000},
    "right-colour-is-left": {0x80104248: 0x24422BD8},
    "slot": {0x801042D4: 0x24840018},
    "additive-page": {0x801042DC: 0x24190040},
}
GRADIENTS = 4
PACKET_BYTES = 36 + 12
# The fifth argument, on the caller's stack.
STACK_ARGUMENT = 0x10


def vector(rng: random.Random, number: int) -> list[int]:
    """[x, y, width, height, gradient]: the help panel's boxes, wrapping corners and gradients
    outside the palette."""
    gradient = number % GRADIENTS if number % 8 else rng.choice((-1, 4, 5, int32(rng, -9, 9)))
    return [int16(rng, -20, 330), int16(rng, -20, 240), int32(rng, -4, 320),
            int32(rng, -4, 240), gradient]


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    x, y, width, height, gradient = vector
    inputs = [OP_BACKGROUND, *vector]
    if not 0 <= gradient < GRADIENTS:
        # The original reads its colours past the palette; the native code refuses.
        return [[], [REFUSED]], inputs
    install_frame(machine, PACKET_BYTES)
    for register, value in zip((4, 5, 6, 7), (x, y, width, height), strict=True):
        machine.r[register] = value & 0xFFFFFFFF
    machine.put(machine.r[29] + STACK_ARGUMENT, gradient)
    machine.execute(ENTRY)
    problems: list[str] = []
    return [problems, [DRAWN, *events(machine, problems)]], inputs


def describe(image, results: list) -> list[str]:
    drawn, refused, queued = counts(results)
    return [f"coverage: {drawn} backgrounds ({queued} packets), {refused} gradients refused"]
