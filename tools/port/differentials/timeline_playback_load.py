"""F01557 func_800D55A4 (BATTLE.PRG 5BF94.c:7885), typed effect timeline load."""

from timeline_playback_differential import LOAD, configure

globals().update(
    configure(
        1557,
        LOAD,
        0x800D55A4,
        0x800D5700,
        {
            "particle-kind-4": {0x800D5670: 0x24020004},
            "child-flag-1": {0x800D56C8: 0x24020001},
            "key-index-starts-0": {0x800D5680: 0x24020000},
            "active-bit-0-only": {0x800D56A4: 0x00042000},
            "next-track-header": {0x800D5660: 0x96620006},
        },
    )
)
