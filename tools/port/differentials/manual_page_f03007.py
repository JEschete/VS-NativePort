"""F03007 Menu::Manual::DrawControlsBackground, MENUE.PRG _drawControlsBg 494.c:962."""

from __future__ import annotations

import random

from differential import Machine
from manual_page_common import (
    DRAWN,
    DRIVER,
    INCLUDES,
    MODULES,
    OP_CONTROLS,
    REFUSED,
    SOURCES,
    check,
    counts,
    events,
    install_frame,
    int16,
    int32,
    signed,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "check")

ID = "F03007"
ENTRY, END = 0x80104CC4, 0x80104E54
VECTORS, SEED = 256, 3007
MUTANT_VECTORS = 128
MUTANTS = {
    "one-more-row": {0x80104CF8: 0x24430000},
    "right-colour": {0x80104D20: 0x24160011},
    "semi-transparent-line": {0x80104D34: 0x24020052},
    "lines-not-shortened": {0x80104D94: 0x26B50000},
    "shadow-offset": {0x80104DAC: 0x26730003},
    "shadow-slant-added": {0x80104DE8: 0x007E1821},
    "line-slot": {0x80104D9C: 0x24840020},
}
MAX_ROWS = 512
LINE_BYTES, QUAD_BYTES = 20, 24


def rows(y: int, height: int) -> int:
    last = signed(y + height - 1)
    return last - y if last > y else 0


def vector(rng: random.Random, number: int) -> list[int]:
    """[x, y, width, height]: the hint boxes, empty and one-row boxes, sums that wrap past
    INT_MAX and heights past the row limit."""
    x, y = int16(rng, -10, 330), int32(rng, -10, 240)
    width = int32(rng, -4, 160)
    height = rng.choice((0, 1, 2, rng.randrange(-3, 40), rng.randrange(1, 24)))
    if number % 16 == 1:
        y = rng.randrange(0x7FFFFFF0, 0x80000000)
    elif number % 16 == 3:
        height = rng.choice((MAX_ROWS, MAX_ROWS + 1, MAX_ROWS + 2, signed(rng.getrandbits(32))))
    return [x, y, width, height]


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    x, y, width, height = vector
    inputs = [OP_CONTROLS, *vector]
    count = rows(y, height)
    if count > MAX_ROWS:
        # The original shades every row however many; the native code refuses past VRAM's height.
        return [[], [REFUSED]], inputs
    install_frame(machine, count * LINE_BYTES + QUAD_BYTES)
    for register, value in zip((4, 5, 6, 7), vector, strict=True):
        machine.r[register] = value & 0xFFFFFFFF
    machine.execute(ENTRY)
    problems: list[str] = []
    return [problems, [DRAWN, *events(machine, problems)]], inputs


def describe(image, results: list) -> list[str]:
    drawn, refused, queued = counts(results)
    empty = sum(1 for vector, (_, expected), _ in results
                if expected[0] == DRAWN and not rows(vector[1], vector[3]))
    return [f"coverage: {drawn} boxes ({queued} packets, {empty} without lines), "
            f"{refused} refused past {MAX_ROWS} rows"]
