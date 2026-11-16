"""F02994 Menu::manual::LastPageLineOffset, MENUE.PRG _getRoundedLineCount 494.c:514."""

from __future__ import annotations

import random

from differential import Machine
from manual_topics_common import (
    CXX_STANDARD,
    DRIVER,
    HELP_TEXT,
    INCLUDES,
    INPUT,
    MODULES,
    OP_LAST_PAGE,
    SOURCES,
    mutants,
    run_original,
    signed,
    unfaulted,
    verify_original_words,
)

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02994"
ENTRY, END = 0x80103B6C, 0x80103BC8
VECTORS, SEED = 512, 2994
MUTANTS = mutants(ID)
EDGES = (0, 1, 9, 10, 11, 19, 20, 21, 0x7FFF, 0x8000, 0x8001, 0xFFF6, 0xFFFF)


def vector(rng: random.Random, number: int) -> list[int]:
    """[help text line count halfword, the halfword after it]."""
    count = EDGES[number] if number < len(EDGES) else rng.choice(
        (rng.randrange(0, 40), rng.randrange(0, 400), rng.randrange(0x10000)))
    return [count, rng.randrange(0x10000)]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    count, following = vector
    machine.put(INPUT + 16, count, 2)
    machine.put(INPUT + 18, following, 2)
    machine.put(HELP_TEXT, INPUT)
    faulted = run_original(machine, ENTRY)
    return [int(faulted), signed(machine.r[2])], [OP_LAST_PAGE, count]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return unfaulted(observation, native)


def describe(image, results: list) -> list[str]:
    verify_original_words(ID, image)
    rounded = sum(1 for vector, _, _ in results if vector[0] % 10)
    clamped = sum(1 for vector, _, _ in results if vector[0] <= 10)
    return [f"coverage: {rounded} counts rounded up, {clamped} clamped to the first page"]
