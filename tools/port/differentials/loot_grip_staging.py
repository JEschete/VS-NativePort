"""F02847 _copyGripToInventory (MENUB.PRG 260.c:739), typed loot staging."""

from __future__ import annotations

import random

from differential import Machine
from loot_staging_differential import (
    COMPILER as COMPILER,
)
from loot_staging_differential import (
    COMPILER_OPTIONS as COMPILER_OPTIONS,
)
from loot_staging_differential import (
    CXX_STANDARD as CXX_STANDARD,
)
from loot_staging_differential import (
    GRIP,
)
from loot_staging_differential import (
    INCLUDES as INCLUDES,
)
from loot_staging_differential import (
    check as check_vector,
)
from loot_staging_differential import run as run_vector
from loot_staging_differential import vector as make_vector

ID = "F02847"
ENTRY, END = 0x80107CE8, 0x80107D7C
MODULES = ("MENU/MENUB.PRG",)
DRIVER = "loot_staging_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Loot\LootStaging.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
    r"Native\Game\src\Items\Inventory.cpp",
)
VECTORS, SEED = 256, 2847
MUTANT_VECTORS = 64
MUTANTS = {
    "grip-owner-not-written": {0x80107D5C: 0x00000000},
    "grip-index-not-written": {0x80107D60: 0x00000000},
}


def vector(rng: random.Random, number: int) -> dict:
    return make_vector(rng, number, GRIP)


def run(machine: Machine, vector_value: dict) -> tuple:
    return run_vector(machine, vector_value, ENTRY)


def check(image, vector_value: dict, observation: tuple,
          native: list[int], reference: tuple) -> list[str]:
    return check_vector(image, vector_value, observation, native, reference)
