"""F03004 Menu::Manual::DrawPageArrow, MENUE.PRG _drawPaginationArrow 494.c:844."""

from __future__ import annotations

import random

from differential import Machine
from manual_page_common import (
    DRAWN,
    DRIVER,
    INCLUDES,
    MODULES,
    OP_ARROW,
    SOURCES,
    arrow_state,
    buttons,
    check,
    counts,
    events,
    install_frame,
    put_arrow,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "check")

ID = "F03004"
ENTRY, END = 0x80104908, 0x80104A44
VECTORS, SEED = 256, 3004
MUTANT_VECTORS = 128
MUTANTS = {
    "direction-swapped": {0x80104914: 0x1480000D},
    "down-bounce-frames": {0x80104928: 0x2862000C},
    "down-top": {0x80104938: 0x244500B9},
    "down-return": {0x80104948: 0x244500BD},
    "up-top": {0x80104960: 0x24030029},
    "up-return": {0x80104974: 0x24450023},
    "right-edge": {0x80104984: 0x240300A9},
    "shaded-texture": {0x8010498C: 0x2402002C},
    "texel-top": {0x801049B8: 0x24030031},
    "texture-page": {0x801049EC: 0x2403000D},
    "square-button": {0x801049F8: 0x30420040},
    "dim-palette": {0x80104A08: 0x240237FA},
    "slot": {0x80104A28: 0x2484FFE8},
}
PACKET_BYTES = 40


def vector(rng: random.Random, number: int) -> list[int]:
    """[arrow type 0 up or 1 down, _pageArrowAnimState, Input_previousButtons]."""
    return [number % 2, arrow_state(rng), buttons(rng)]


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    arrow, state, held = vector
    install_frame(machine, PACKET_BYTES)
    square = put_arrow(machine, state, held)
    machine.r[4] = arrow
    machine.execute(ENTRY)
    problems: list[str] = []
    observation = [DRAWN, *events(machine, problems)]
    return [problems, observation], [OP_ARROW, arrow, state, square]


def describe(image, results: list) -> list[str]:
    down = sum(1 for vector, _, _ in results if vector[0])
    drawn, _, queued = counts(results)
    return [f"coverage: {drawn} arrows ({down} down, {drawn - down} up), {queued} packets"]
