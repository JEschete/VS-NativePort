"""F03003 Menu::Manual::DrawLowerScrollFade, MENUE.PRG _fadeMenuLower 494.c:791."""

from __future__ import annotations

import random

from differential import Machine
from manual_page_common import (
    DRAW_PAGINATION_ARROW,
    DRIVER,
    INCLUDES,
    MODULES,
    OP_LOWER_FADE,
    SOURCES,
    check,
    fade_describe,
    fade_vector,
    run_fade,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "check")

ID = "F03003"
ENTRY, END = 0x80104620, 0x80104908
VECTORS, SEED = 96, 3003
MUTANT_VECTORS = 24
MUTANTS = {
    "up-arrow": {0x801046AC: 0x24040000},
    "first-row": {0x801046C0: 0x240600BA},
    "palette": {0x80104758: 0x240238F2},
    "page-bits": {0x80104764: 0x3442009B},
    "two-chunks": {0x801047C0: 0x2A220002},
    "first-tile-row": {0x801047F0: 0x240200BC},
    "subtractive-page": {0x80104868: 0x24070060},
}


def vector(rng: random.Random, number: int) -> list[int]:
    return fade_vector(rng)


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    machine.follow.add(DRAW_PAGINATION_ARROW)
    return run_fade(machine, vector, ENTRY, OP_LOWER_FADE)


def describe(image, results: list) -> list[str]:
    return fade_describe(results)
