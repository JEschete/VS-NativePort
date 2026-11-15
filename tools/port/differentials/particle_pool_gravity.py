"""F01552 func_800D5260 (BATTLE.PRG 5BF94.c:7826), typed particle gravity."""

from particle_pool_differential import GRAVITY, configure

globals().update(
    configure(
        1552,
        GRAVITY,
        0x800D5260,
        0x800D5294,
        {
            "x-from-y": {0x800D5268: 0x8C830004},
            "y-kept": {0x800D5278: 0x00000000},
            "fourth-kept": {0x800D5288: 0x00000000},
        },
    )
)
