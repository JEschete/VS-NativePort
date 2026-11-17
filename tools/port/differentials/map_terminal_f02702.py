"""F02702 _applyPalingScreenEffect, MENU5.PRG 4D8.c:1880; typed map::DrawPalingDistortion."""

from map_terminal_common import PALING, configure

globals().update(configure(2702, PALING, 0x801066E0, 0x80106AF4, {
    "240-not-239-rows": {0x80106AC8: 0x2A2200F0},
    "brightness": {0x8010676C: 0x2412007F},
    "even-row-sign": {0x801067F0: 0},
    "negative-odd-round-down": {0x801067C0: 0},
    "left-height": {0x80106800: 0x24020002},
    "wrong-strip-slot": {0x80106824: 0x24840024},
    "right-width": {0x801069A4: 0x2402007F},
    "right-clut-cleared": {0x80106960: 0xA600000E},
}))
