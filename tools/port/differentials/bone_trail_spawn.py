"""F01447 func_800CF7A8 (BATTLE.PRG 5BF94.c:5666), the bone trail request."""

from bone_trail_differential import SPAWN, configure

globals().update(
    configure(
        1447,
        SPAWN,
        0x800CF7A8,
        0x800CF830,
        {
            "effect-id": {0x800CF7AC: 0x24020037},
            "color-nibble": {0x800CF7D8: 0x30C60007},
            "texture-shift": {0x800CF7E8: 0x00073C00},
            "life-byte": {0x800CF7F0: 0x3C057F00},
            "target-type": {0x800CF7C4: 0xA7A20022},
        },
    )
)
