"""F04158 Menu::manual::SetTopicRows, MENUE.PRG _setMenuRows 494.c:76."""

from __future__ import annotations

import random

from differential import Machine
from manual_topics_common import (
    CXX_STANDARD,
    DRIVER,
    INCLUDES,
    MODULES,
    NO_ROWS,
    OP_SET_ROWS,
    SET_TOPIC_ROWS,
    SOURCES,
    STRINGS,
    TOPIC_TABLE,
    any_byte,
    any_int,
    compare,
    install_menu,
    mutants,
    rows_observation,
    run_original,
    untouched_rows,
    verify_original_words,
)

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F04158"
ENTRY, END = SET_TOPIC_ROWS, 0x80102DF0
VECTORS, SEED = 512, 4158
MUTANTS = mutants(ID)


def vector(rng: random.Random, number: int) -> list[int]:
    """[row count, highlighted row, cursor memory setting, cursor row, cursor scroll, string offsets...]."""
    count = rng.choice((1, 2, 8, 9, 10, 14, 14, 17, rng.randrange(1, 31), rng.randrange(1, 31), 0, -1))
    highlighted = rng.choice((count - 9, count - 8, count - 1, 7, 8, 4, any_int(rng, -2, count + 2)))
    offsets = [rng.randrange(0, 0x800) for _ in range(max(count, 0) * 2)]
    return [count, highlighted, any_byte(rng), any_byte(rng), any_byte(rng), *offsets]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    count, highlighted, setting, row, scroll, *offsets = vector
    record = install_menu(machine, row, scroll, setting)
    machine.map_zero(STRINGS, 0x400)
    for index, offset in enumerate(offsets):
        machine.put(STRINGS + index * 4, TOPIC_TABLE + offset)
    machine.r[4], machine.r[5], machine.r[6] = count & 0xFFFFFFFF, highlighted & 0xFFFFFFFF, STRINGS
    faulted = run_original(machine, ENTRY)
    observation = [int(faulted), *rows_observation(machine, record)]
    return observation, [OP_SET_ROWS, count, highlighted, setting, row, scroll, len(offsets), *offsets]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    count, _highlighted, setting, row, scroll, *_ = vector
    if count < 1:
        # The original writes rowTypes[-1] and lists no rows; the native code refuses untouched.
        return compare(native, [NO_ROWS, *untouched_rows(row, scroll, setting)])
    if observation[0]:
        return ["the original faulted"]
    return compare(native, [0, *observation[1:]])


def describe(image, results: list) -> list[str]:
    verify_original_words(ID, image)
    scrolled = sum(1 for _, observation, _ in results if observation[2])
    middle = sum(1 for vector, observation, _ in results if vector[0] >= 1 and observation[1] == 4 and observation[2])
    refused = sum(1 for vector, _, _ in results if vector[0] < 1)
    return [f"coverage: {scrolled} vectors scrolled the list ({middle} centred on row 4), {refused} had no rows"]
