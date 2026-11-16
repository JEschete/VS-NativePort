"""F00731 awareness and talk markers: kinds, pinned kinds, side bits, slot clamps, edge arrow."""

from functools import partial

from differentials.actor_markers_common import (
    MUTATIONS,
    SPECS,
    check_for,
    run_for,
    vector_for,
)

ID = "F00731"
_, ENTRY, END = SPECS[ID]
DRIVER = "actor_markers_driver.cpp"
SOURCES = ("Native\\Game\\src\\Battle\\ActorMarkers.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 1024, 731, 256
MUTANTS = MUTATIONS[ID]
vector = partial(vector_for, ID)
run = partial(run_for, ID)
check = partial(check_for, ID)
