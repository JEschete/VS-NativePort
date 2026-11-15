"""F01507 func_800D27F0 (BATTLE.PRG 5BF94.c:6818), typed particle allocation."""

from particle_pool_differential import ALLOCATE, configure

globals().update(
    configure(
        1507,
        ALLOCATE,
        0x800D27F0,
        0x800D2888,
        {
            "texture-skipped": {0x800D281C: 0x00000000},
            "age-0xfe": {0x800D2824: 0x240200FE},
            "previous-kept": {0x800D282C: 0x00000000},
            "used-kept": {0x800D2854: 0x00000000},
            "old-first-unlinked": {0x800D2858: 0x00000000},
            "first-kept": {0x800D2860: 0x00000000},
            "count-step-two": {0x800D2864: 0x24420002},
        },
    )
)
