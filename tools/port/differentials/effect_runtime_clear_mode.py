"""F01450 func_800CF920 (BATTLE.PRG 5BF94.c:5712), typed action variant clear."""

from effect_runtime_differential import CLEAR_MODE, configure

globals().update(
    configure(
        1450,
        CLEAR_MODE,
        0x800CF920,
        0x800CF92C,
        {
            "next-byte": {0x800CF928: 0xA040522D},
            "store-dropped": {0x800CF928: 0x00000000},
            "halfword": {0x800CF928: 0xA440522C},
        },
    )
)
