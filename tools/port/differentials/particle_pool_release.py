"""F01506 func_800D278C (BATTLE.PRG 5BF94.c:6805), typed particle pool release."""

from particle_pool_differential import RELEASE, configure

globals().update(
    configure(
        1506,
        RELEASE,
        0x800D278C,
        0x800D27F0,
        {
            "free-skipped": {0x800D27C0: 0x00000000},
            "block-kept": {0x800D27C8: 0x00000000},
            "last-reference-kept": {0x800D27D0: 0x00000000},
            "decrement-two": {0x800D27D4: 0x2462FFFE},
            "decrement-kept": {0x800D27D8: 0x00000000},
        },
    )
)
