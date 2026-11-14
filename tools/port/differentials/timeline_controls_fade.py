"""F01575 func_800D64E4 (BATTLE.PRG 5BF94.c:8247), typed weapon trail fade."""

from timeline_controls_differential import FADE, configure

globals().update(
    configure(
        1575,
        FADE,
        0x800D64E4,
        0x800D6508,
        {
            "fade-argument-1": {0x800D64E8: 0x24040001},
            "fade-skipped": {0x800D64F0: 0x00000000},
        },
    )
)
