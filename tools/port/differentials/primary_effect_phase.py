"""F01440 func_800CF48C (BATTLE.PRG 5BF94.c:5550), typed phase read."""

from primary_effect_differential import READ_PHASE, configure

globals().update(
    configure(
        1440,
        READ_PHASE,
        0x800CF48C,
        0x800CF49C,
        {
            "phase-halfword": {0x800CF490: 0x8442531C},
            "phase-misread": {0x800CF490: 0x8C425320},
        },
    )
)
