"""F01514 func_800D2A38 (BATTLE.PRG 5BF94.c:6915), a particle spawn position."""

from particle_motion_differential import SPAWN, configure

globals().update(
    configure(
        1514,
        SPAWN,
        0x800D2A38,
        0x800D2ADC,
        {
            "flag-bit": {0x800D2A50: 0x3C030008},
            "position-shift": {0x800D2A94: 0x000212C0},
            "origin-y": {0x800D2AA4: 0x8E030098},
            "offset-z": {0x800D2A7C: 0x8604002E},
        },
    )
)
