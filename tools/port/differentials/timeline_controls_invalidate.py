"""F01580 func_800D65A8 (BATTLE.PRG 5BF94.c:8266), typed transform attachment invalidation."""

from timeline_controls_differential import INVALIDATE, configure

globals().update(
    configure(
        1580,
        INVALIDATE,
        0x800D65A8,
        0x800D65D8,
        {
            "invalidate-keeps-target": {0x800D65CC: 0x3042FFFE},
            "transform-stride-0x68": {0x800D65BC: 0x000210C0},
            "index-unmasked": {0x800D65A8: 0x00000000},
        },
    )
)
