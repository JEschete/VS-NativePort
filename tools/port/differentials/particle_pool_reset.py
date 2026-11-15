"""F01504 func_800D268C (BATTLE.PRG 5BF94.c:6777), typed particle pool reference reset."""

from particle_pool_differential import RESET, configure

globals().update(
    configure(
        1504,
        RESET,
        0x800D268C,
        0x800D2698,
        {
            "references-kept": {0x800D2694: 0x00000000},
            "pool-cleared": {0x800D2694: 0xAC4055FC},
        },
    )
)
