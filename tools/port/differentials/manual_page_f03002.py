"""F03002 Menu::Manual::DrawUpperScrollFade, MENUE.PRG _fadeMenuUpper 494.c:740."""

from __future__ import annotations

import random

from differential import Machine
from manual_page_common import (
    DRAW_PAGINATION_ARROW,
    DRIVER,
    INCLUDES,
    MODULES,
    OP_UPPER_FADE,
    SOURCES,
    check,
    fade_describe,
    fade_vector,
    run_fade,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "check")

ID = "F03002"
ENTRY, END = 0x8010435C, 0x80104620
VECTORS, SEED = 96, 3002
MUTANT_VECTORS = 24
MUTANTS = {
    "down-arrow": {0x801043E4: 0x24040001},
    "first-row": {0x801043FC: 0x26540035},
    "palette": {0x8010447C: 0x240238F1},
    "page-bits": {0x8010448C: 0x3442009B},
    "opaque-chunks": {0x80104498: 0x34630000},
    "two-chunks": {0x801044DC: 0x2A220002},
    "first-tile-row": {0x80104510: 0x26420033},
    "tile-width": {0x80104518: 0x2402013F},
    "subtractive-page": {0x80104584: 0x24070060},
}


def vector(rng: random.Random, number: int) -> list[int]:
    return fade_vector(rng)


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    machine.follow.add(DRAW_PAGINATION_ARROW)
    return run_fade(machine, vector, ENTRY, OP_UPPER_FADE)


def describe(image, results: list) -> list[str]:
    return fade_describe(results)
