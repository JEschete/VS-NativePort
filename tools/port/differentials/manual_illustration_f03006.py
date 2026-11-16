"""F03006 Menu::manual::AdvanceVisibilityAnimation, MENUE.PRG _getAnimationState 494.c:940."""

from __future__ import annotations

import random

from differential import Machine
from manual_illustration_common import (
    DRIVER,
    INCLUDES,
    INPUT,
    MODULES,
    OP_ANIMATION,
    SOURCES,
    animation,
    compare,
    load_halfwords,
    put_halfwords,
    signed,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F03006"
ENTRY, END = 0x80104C30, 0x80104CC4
VECTORS, SEED = 1024, 3006
MUTANTS = {
    "frame-limit": {0x80104C38: 0x2862000F},
    "mask-bit": {0x80104C5C: 0x30450003},
    "timer-step": {0x80104C68: 0x24630002},
    "duration-compare-swapped": {0x80104C78: 0x0043182A},
    "visible-not-inverted": {0x80104C80: 0x00A01021},
    "repeat-ignored": {0x80104CB8: 0x00000000},
}
HALFWORDS = 7


def vector(rng: random.Random, number: int) -> list[int]:
    """The seven AnimationState_t halfwords."""
    return animation(rng)


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    machine.map_zero(INPUT, 16)
    put_halfwords(machine, INPUT, vector)
    machine.r[4] = INPUT
    machine.execute(ENTRY)
    return [signed(machine.r[2]), *load_halfwords(machine, INPUT, HALFWORDS)], \
        [OP_ANIMATION, *vector]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return compare(native, observation)
