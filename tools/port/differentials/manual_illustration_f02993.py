"""F02993 Menu::manual::UploadCluts, MENUE.PRG _copyCluts 494.c:498."""

from __future__ import annotations

import random

from differential import Machine
from manual_illustration_common import (
    DRIVER,
    ENTRY_OF,
    INCLUDES,
    MODULES,
    OP_UPLOAD_CLUTS,
    SOURCES,
    check_upload,
    run_upload,
    upload_vector,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02993"
ENTRY, END = ENTRY_OF[OP_UPLOAD_CLUTS], 0x80103B6C
VECTORS, SEED = 512, 2993
MUTANT_VECTORS = 128
MUTANTS = {
    "clut-height": {0x80103AFC: 0x24020002},
    "grid-x": {0x80103B28: 0x24630310},
    "grid-y": {0x80103B2C: 0x244201F1},
    "clut-stride": {0x80103B50: 0x26310024},
    "zero-count-uploads": {0x80103B04: 0x06400013},
}
LIMIT = 256


def vector(rng: random.Random, number: int) -> list[int]:
    """[count]: palettes to upload; past 256 the native code refuses."""
    return upload_vector(rng, number, LIMIT,
                         (-0x80000000, -2, -1, 0, 1, 2, 15, 16, 17, 112, 255, 256, 257), 40)


def run(machine: Machine, vector: list[int]) -> tuple[list[list[int]], list[int]]:
    return run_upload(machine, vector[0], OP_UPLOAD_CLUTS, 8)


def check(image, vector: list[int], observation: list[list[int]], native: list[int],
          reference: list[list[int]]) -> list[str]:
    return check_upload(vector[0], LIMIT, observation, native)
