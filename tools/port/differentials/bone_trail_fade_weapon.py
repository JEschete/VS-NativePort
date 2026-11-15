"""F00057 func_8006C4A4 (BATTLE.PRG 146C.c:1925), the weapon trail fade."""

from bone_trail_differential import WEAPON_FADE, configure

globals().update(
    configure(
        57,
        WEAPON_FADE,
        0x8006C4A4,
        0x8006C4C4,
        {
            "fade-skipped": {0x8006C4AC: 0x00000000},
        },
    )
)
