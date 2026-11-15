"""F01434 func_800CF1A8 (BATTLE.PRG 5BF94.c:5462), typed primary effect clear."""

from effect_runtime_differential import CLEAR_PRIMARY, configure

globals().update(
    configure(
        1434,
        CLEAR_PRIMARY,
        0x800CF1A8,
        0x800CF218,
        {
            "vibration-skipped": {0x800CF1C4: 0x00000000},
            "free-skipped": {0x800CF1E4: 0x00000000},
            "timeline-kept": {0x800CF1EC: 0x00000000},
            "first-only": {0x800CF1FC: 0x00000000},
        },
    )
)
