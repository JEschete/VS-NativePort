"""F00056 func_8006C480 (BATTLE.PRG 146C.c:1923), the weapon trail start."""

from bone_trail_differential import WEAPON_SPAWN, configure

globals().update(
    configure(
        56,
        WEAPON_SPAWN,
        0x8006C480,
        0x8006C4A4,
        {
            "color-argument": {0x8006C484: 0x00803021},
            "texture-argument": {0x8006C490: 0x00A03821},
        },
    )
)
