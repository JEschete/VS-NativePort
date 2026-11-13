"""F01361 shortcut panel animation, callback order and packed presentation."""

from functools import partial

from differentials.hud_orchestration_common import (
    MUTATIONS,
    SPECS,
    check_for,
    run_for,
    vector_for,
)

ID = "F01361"
_, ENTRY, END = SPECS[ID]
DRIVER = "hud_orchestration_driver.cpp"
SOURCES = (
    "Native\\Game\\src\\Battle\\HudOrchestration.cpp",
    "Native\\Game\\src\\Battle\\HudState.cpp",
    "Native\\Engine\\src\\Core\\Math\\ScalarMath.cpp",
    "Native\\Engine\\src\\Core\\Math\\Distance.cpp",
    "Native\\Engine\\src\\Render\\Sprites.cpp",
)
INCLUDES = ("Native\\Tests\\include",)
VECTORS, SEED, MUTANT_VECTORS = 1024, 1361, 256
MUTANTS = MUTATIONS[ID]
vector = partial(vector_for, ID)
run = partial(run_for, ID)
check = partial(check_for, ID)
