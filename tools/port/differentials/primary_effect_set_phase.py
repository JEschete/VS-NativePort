"""F01438 func_800CF478 (BATTLE.PRG 5BF94.c:5546), typed phase store."""

from primary_effect_differential import SET_PHASE, configure

globals().update(
    configure(
        1438,
        SET_PHASE,
        0x800CF478,
        0x800CF484,
        {
            "phase-zeroed": {0x800CF480: 0xAC40531C},
            "phase-misplaced": {0x800CF480: 0xAC445320},
        },
    )
)
