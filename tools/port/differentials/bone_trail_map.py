"""F01452 func_800CF988 (BATTLE.PRG 5BF94.c:5738), the trail framebuffer texture."""

from bone_trail_differential import MAP, configure

globals().update(
    configure(
        1452,
        MAP,
        0x800CF988,
        0x800CFAAC,
        {
            "display-width": {0x800CF9A4: 0x2403013F},
            "page-mask": {0x800CF9F8: 0x2402FFE0},
            "second-buffer": {0x800CFA28: 0x24820004},
            "direct-color": {0x800CFA38: 0x34420080},
            "corner-3": {0x800CFA88: 0x8E260024},
            "offset-sign": {0x800CFA3C: 0x00A38021},
        },
    )
)
