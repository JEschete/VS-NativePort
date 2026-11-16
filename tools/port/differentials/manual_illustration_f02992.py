"""F02992 Menu::manual::UploadSpriteTiles, MENUE.PRG _copySprites 494.c:481."""

from __future__ import annotations

import random

from differential import Machine
from manual_illustration_common import (
    DRIVER,
    ENTRY_OF,
    INCLUDES,
    MODULES,
    OP_UPLOAD_SPRITES,
    SOURCES,
    check_upload,
    run_upload,
    upload_vector,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02992"
ENTRY, END = ENTRY_OF[OP_UPLOAD_SPRITES], 0x80103AD8
VECTORS, SEED = 512, 2992
MUTANT_VECTORS = 128
MUTANTS = {
    "tile-width": {0x80103A40: 0x24020008},
    "atlas-x": {0x80103A94: 0x24420344},
    "atlas-row-shift": {0x80103A78: 0x00061903},
    "tile-stride": {0x80103AB8: 0x26310084},
    "zero-count-uploads": {0x80103A54: 0x06400019},
}
LIMIT = 768


def vector(rng: random.Random, number: int) -> list[int]:
    """[count]: tiles to upload; past 768 the native code refuses."""
    return upload_vector(rng, number, LIMIT,
                         (-0x80000000, -2, -1, 0, 1, 2, 47, 48, 49, 96, 767, 768, 769), 100)


def run(machine: Machine, vector: list[int]) -> tuple[list[list[int]], list[int]]:
    return run_upload(machine, vector[0], OP_UPLOAD_SPRITES, 32)


def check(image, vector: list[int], observation: list[list[int]], native: list[int],
          reference: list[list[int]]) -> list[str]:
    return check_upload(vector[0], LIMIT, observation, native)
