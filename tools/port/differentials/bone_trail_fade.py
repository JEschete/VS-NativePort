"""F01448 func_800CF830 (BATTLE.PRG 5BF94.c:5681), the bone trail fade."""

from bone_trail_differential import FADE, configure

globals().update(
    configure(
        1448,
        FADE,
        0x800CF830,
        0x800CF8BC,
        {
            "renderer-test": {0x800CF870: 0x10520004},
            "fade-command": {0x800CF878: 0x24050004},
            "first-slot": {0x800CF858: 0x8E300D54},
            "next-instance": {0x800CF894: 0x8E310004},
        },
    )
)
