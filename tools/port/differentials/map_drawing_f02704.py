"""F02704 _drawUIControls (MENU5.PRG 4D8.c:1991), typed Menu::map::DrawControls."""

from map_drawing_common import CONTROLS, configure

globals().update(
    configure(
        2704,
        CONTROLS,
        0x80106C84,
        0x801074AC,
        {
            "slide-start-0x70": {0x80106CB8: 0x2403FF90},
            "slide-in-step-8": {0x80106CE8: 0x24420008},
            "box-shift-down": {0x80106CFC: 0x24420008},
            "slide-out-bound": {0x80106D1C: 0x2862FF82},
            "hide-ignored": {0x80106D58: 0x00000000},
            "r1-x-0x48": {0x80106DB4: 0x24A50048},
            "back-x-0x20": {0x80106E6C: 0x24A50020},
            "backdrop-width-0x50": {0x80107394: 0x24060050},
            "tpage-0x102": {0x801073DC: 0x24190102},
            "compass-y-0xb0": {0x80107454: 0x240500B0},
        },
    )
)
