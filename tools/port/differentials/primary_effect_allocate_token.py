"""F01441 func_800CF49C (BATTLE.PRG 5BF94.c:5552), typed effect token allocation."""

from primary_effect_differential import ALLOCATE, configure

globals().update(
    configure(
        1441,
        ALLOCATE,
        0x800CF49C,
        0x800CF514,
        {
            "allocates-bit-0": {0x800CF4C4: 0x24040000},
            "skips-bit-31": {0x800CF4F0: 0x2882001F},
            "three-words": {0x800CF500: 0x28E20003},
            "token-drops-word": {0x800CF4E8: 0x00045021},
        },
    )
)
