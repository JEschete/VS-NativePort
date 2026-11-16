"""F02996 Menu::manual::CopyEncodedLine, MENUE.PRG _vsStrcpy 494.c:561."""

from __future__ import annotations

import random

from differential import Machine
from manual_topics_common import (
    COPY_ENCODED_LINE,
    CXX_STANDARD,
    DESTINATION,
    DRIVER,
    INCLUDES,
    INPUT,
    MODULES,
    OP_COPY_LINE,
    PAGE_OVERFLOW,
    SOURCES,
    UNTERMINATED_LINE,
    byte_stores,
    compare,
    copied_length,
    encoded_line,
    mutants,
    run_original,
    verify_original_words,
)

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02996"
ENTRY, END = COPY_ENCODED_LINE, 0x80103CF0
VECTORS, SEED = 512, 2996
MUTANTS = mutants(ID)


def vector(rng: random.Random, number: int) -> list[int]:
    """[destination capacity, source bytes...]: the source ends where the overlay ends."""
    length = rng.choice((0, 1, 2, rng.randrange(0, 8), rng.randrange(0, 40)))
    terminated = rng.randrange(6) != 0
    line = encoded_line(rng, length, terminated)
    if terminated:
        line += [rng.randrange(256) for _ in range(rng.randrange(3))]
    needed = copied_length(line) if terminated else len(line) + 2
    capacity = rng.choice((needed, needed - 1, needed + 1, needed + rng.randrange(40), rng.randrange(0, needed + 1)))
    return [max(capacity, 0), *line]


def run(machine: Machine, vector: list[int]) -> tuple[dict, list[int]]:
    capacity, *source = vector
    machine.store(INPUT, bytes(source))
    machine.r[4], machine.r[5] = DESTINATION, INPUT
    faulted = run_original(machine, ENTRY)
    stores = byte_stores(machine)
    written = (machine.r[2] - DESTINATION) & 0xFFFFFFFF
    copied = [] if faulted or written > 0x1000 else list(machine.bytes(DESTINATION, written))
    observation = {"faulted": faulted, "overflow": any(address >= DESTINATION + capacity for address in stores),
                   "copied": copied}
    return observation, [OP_COPY_LINE, capacity, len(source), *source]


def check(image, vector: list[int], observation: dict, native: list[int],
          reference: dict) -> list[str]:
    if observation["overflow"]:
        # The original writes past the destination; the native code refuses at that write.
        return compare(native, [PAGE_OVERFLOW])
    if observation["faulted"]:
        # The original reads past its line; the native code refuses at that read.
        return compare(native, [UNTERMINATED_LINE])
    return compare(native, [0, len(observation["copied"]), *observation["copied"]])


def describe(image, results: list) -> list[str]:
    verify_original_words(ID, image)
    overflow = sum(1 for _, observation, _ in results if observation["overflow"])
    unterminated = sum(1 for _, observation, _ in results if observation["faulted"] and not observation["overflow"])
    codes = sum(1 for vector, observation, _ in results if not observation["faulted"]
                and any(value >= 0xEC for value in observation["copied"]))
    return [f"coverage: {overflow} overflowed, {unterminated} unterminated, {codes} copied two-byte codes"]
