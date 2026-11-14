"""F01561 func_800D57FC (BATTLE.PRG 5BF94.c:7950), typed particle emission track tick."""

from timeline_playback_differential import PARTICLE, configure

globals().update(
    configure(
        1561,
        PARTICLE,
        0x800D57FC,
        0x800D5904,
        {
            "due-compare-swapped": {0x800D583C: 0x0082102A},
            "event-5-bits": {0x800D5878: 0x30A5001F},
            "held-flag-bit-1": {0x800D58A8: 0x30420002},
            "curve-frame-plus-2": {0x800D5890: 0x24420002},
            "curve-frame-not-reset": {0x800D58E0: 0x00000000},
        },
    )
)
