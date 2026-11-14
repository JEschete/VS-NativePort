"""F01576 func_800D6508 (BATTLE.PRG 5BF94.c:8249), typed transform motion resume."""

from timeline_controls_differential import RESUME, configure

globals().update(
    configure(
        1576,
        RESUME,
        0x800D6508,
        0x800D6538,
        {
            "resume-sets-bit-2": {0x800D652C: 0x34420002},
            "resume-reads-0x20": {0x800D6524: 0x94820020},
            "transform-stride-0x68": {0x800D651C: 0x000210C0},
            "index-unmasked": {0x800D6508: 0x00000000},
        },
    )
)
