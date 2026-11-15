"""F01417 func_800CDCBC (BATTLE.PRG 5BF94.c:4898), the bone trail segment draw."""

from bone_trail_differential import DRAW, configure

globals().update(
    configure(
        1417,
        DRAW,
        0x800CDCBC,
        0x800CE174,
        {
            "depth-gate": {0x800CDD34: 0x2C4207F0},
            "distortion-bias": {0x800CDDD8: 0x2502FF81},
            "subtractive-page": {0x800CDF14: 0x24040048},
            "texture-column": {0x800CDF70: 0x00021080},
            "additive-round": {0x800CDFC8: 0x24420000},
            "tip-weight": {0x800CDE60: 0x00520018},
            "distortion-link": {0x800CDEF8: 0x3C06FE00},
            "textured-link": {0x800CE104: 0x3C06FE00},
        },
    )
)
