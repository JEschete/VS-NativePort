"""F01509 func_800D2904 (BATTLE.PRG 5BF94.c:6865), typed particle list release."""

from particle_pool_differential import RELEASE_ALL, configure

globals().update(
    configure(
        1509,
        RELEASE_ALL,
        0x800D2904,
        0x800D2970,
        {
            "free-kept": {0x800D2944: 0x00000000},
            "tail-unlinked": {0x800D2948: 0x00000000},
            "count-signed": {0x800D2950: 0x80C30008},
            "used-kept": {0x800D2960: 0x00000000},
            "count-kept": {0x800D2964: 0x00000000},
        },
    )
)
