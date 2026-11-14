"""F02646 _drawGemInfo versus typed gem status details."""

import runpy
from pathlib import Path

COMMON = runpy.run_path(str(Path(__file__).with_name("equipment_stats_common.py")))

ID = "F02646"
ENTRY, END = 0x80102EC0, 0x80102F64
MODULES = (COMMON["MODULE"],)
DRIVER = "equipment_stats_driver.cpp"
SOURCES = ("Native\\Game\\src\\Menu\\Status\\EquipmentStats.cpp",)
INCLUDES = COMMON["INCLUDES"]
COMPILER = COMMON["COMPILER"]
CXX_STANDARD = COMMON["CXX_STANDARD"]
COMPILER_OPTIONS = COMMON["COMPILER_OPTIONS"]
VECTORS, SEED = 2048, 2646
MUTANTS = {"gem-subtype": {0x80102ECC: 0x24030000}}


def vector(rng, number):
    return COMMON["vector"](rng, number, 6)


def run(machine, case_vector):
    return COMMON["run"](machine, case_vector, ENTRY)


def check(image, case_vector, observation, native, reference):
    return COMMON["check"](image, case_vector, observation, native, reference)
