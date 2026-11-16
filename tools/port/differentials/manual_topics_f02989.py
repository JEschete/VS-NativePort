"""F02989 Menu::manual::GetSelectedTopicRow, MENUE.PRG _getSelectedRow 494.c:104."""

from __future__ import annotations

import random

from differential import Machine
from manual_topics_common import (
    CXX_STANDARD,
    DRIVER,
    GET_SELECTED_TOPIC_ROW,
    INCLUDES,
    MODULES,
    OP_SELECT_ROW,
    SOURCES,
    any_byte,
    install_menu,
    mutants,
    run_original,
    selection_observation,
    signed,
    unfaulted,
    verify_original_words,
)

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02989"
ENTRY, END = GET_SELECTED_TOPIC_ROW, 0x80102E48
VECTORS, SEED = 512, 2989
MUTANTS = mutants(ID)


def vector(rng: random.Random, number: int) -> list[int]:
    """[framework selected row, remembered cursor row]."""
    row = rng.choice((-1, -1, -2, -3, 0, 1, 13, rng.randrange(-40, 40), signed(rng.getrandbits(32))))
    return [row, any_byte(rng)]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    row, cursor = vector
    record = install_menu(machine, cursor, 0, 0, row)
    faulted = run_original(machine, ENTRY)
    observation = [int(faulted), signed(machine.r[2]), *selection_observation(record)]
    return observation, [OP_SELECT_ROW, row, cursor]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return unfaulted(observation, native)


def describe(image, results: list) -> list[str]:
    verify_original_words(ID, image)
    chosen = sum(1 for vector, _, _ in results if vector[0] >= 0)
    cancelled = sum(1 for vector, _, _ in results if vector[0] < -1)
    undecided = sum(1 for vector, _, _ in results if vector[0] == -1)
    return [f"coverage: {chosen} chosen, {cancelled} cancelled, {undecided} undecided"]
