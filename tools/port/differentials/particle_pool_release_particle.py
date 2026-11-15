"""F01508 func_800D2888 (BATTLE.PRG 5BF94.c:6847), typed particle release."""

from particle_pool_differential import RELEASE_PARTICLE, configure

globals().update(
    configure(
        1508,
        RELEASE_PARTICLE,
        0x800D2888,
        0x800D2904,
        {
            "next-back-kept": {0x800D28A4: 0x00000000},
            "previous-forward-kept": {0x800D28C0: 0x00000000},
            "first-kept": {0x800D28CC: 0x00000000},
            "free-kept": {0x800D28E0: 0x00000000},
            "used-plus": {0x800D28E4: 0x24420001},
            "free-link-kept": {0x800D28E8: 0x00000000},
            "count-kept": {0x800D2900: 0x00000000},
        },
    )
)
