"""F02642 _drawArmorInfo versus typed armor status details."""

import runpy
from pathlib import Path

COMMON = runpy.run_path(str(Path(__file__).with_name("equipment_stats_common.py")))

ID = "F02642"
ENTRY, END = 0x80102B70, 0x80102CAC
MODULES = (COMMON["MODULE"],)
DRIVER = "equipment_stats_driver.cpp"
SOURCES = ("Native\\Game\\src\\Menu\\Status\\EquipmentStats.cpp",)
INCLUDES = COMMON["INCLUDES"]
COMPILER = COMMON["COMPILER"]
CXX_STANDARD = COMMON["CXX_STANDARD"]
COMPILER_OPTIONS = COMMON["COMPILER_OPTIONS"]
VECTORS, SEED = 2048, 2642
MUTANTS = {"armor-subtype": {0x80102B80: 0x24030000}}


def vector(rng, number):
    return COMMON["vector"](rng, number, 2)


def run(machine, case_vector):
    return COMMON["run"](machine, case_vector, ENTRY)


def check(image, case_vector, observation, native, reference):
    return COMMON["check"](image, case_vector, observation, native, reference)
