"""F01591 func_800D6CF0 (BATTLE.PRG 5BF94.c:8513), the texture animation restart."""

from particle_motion_differential import INIT, configure

globals().update(
    configure(
        1591,
        INIT,
        0x800D6CF0,
        0x800D6D24,
        {
            "texture-byte": {0x800D6CF4: 0xA0850017},
            "entry-stride": {0x800D6D04: 0x00063080},
            "entry-base": {0x800D6D0C: 0x94C30006},
            "cursor-kept": {0x800D6D10: 0x00000000},
            "timer-kept": {0x800D6D14: 0x00000000},
        },
    )
)
