"""F02644 _drawBladeInfo versus typed blade status details."""

import runpy
from pathlib import Path

COMMON = runpy.run_path(str(Path(__file__).with_name("equipment_stats_common.py")))

ID = "F02644"
ENTRY, END = 0x80102D64, 0x80102E3C
MODULES = (COMMON["MODULE"],)
DRIVER = "equipment_stats_driver.cpp"
SOURCES = ("Native\\Game\\src\\Menu\\Status\\EquipmentStats.cpp",)
INCLUDES = COMMON["INCLUDES"]
COMPILER = COMMON["COMPILER"]
CXX_STANDARD = COMMON["CXX_STANDARD"]
COMPILER_OPTIONS = COMMON["COMPILER_OPTIONS"]
VECTORS, SEED = 2048, 2644
MUTANTS = {"blade-subtype": {0x80102D78: 0x24030000}}


def vector(rng, number):
    return COMMON["vector"](rng, number, 4)


def run(machine, case_vector):
    return COMMON["run"](machine, case_vector, ENTRY)


def check(image, case_vector, observation, native, reference):
    return COMMON["check"](image, case_vector, observation, native, reference)
