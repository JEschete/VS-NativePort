"""F02816 _renderTitleNo, MENU9.PRG 238.c:1835."""

from gazette_statistics_common import DRAW_NUMBER, configure

globals().update(configure(2816, DRAW_NUMBER, 0x80105A0C, 0x80105BCC, {
    "right-offset-eighteen": {0x80105A50: 0x26310012},
    "draw-tens-only": {0x80105A54: 0x24120000},
    "raise-three": {0x80105A58: 0x26D4FFFD},
    "height-nine": {0x80105A64: 0x24150009},
    "wrong-packet-length": {0x80105A6C: 0x24020008},
    "width-five": {0x80105A78: 0x26240005},
    "bottom-five": {0x80105A7C: 0x26C30005},
    "opaque-digits": {0x80105A84: 0x34420000},
    "raw-texture": {0x80105A88: 0x34420001},
    "units-uv-bias": {0x80105AC0: 0x2463FFD1},
    "right-uv-five": {0x80105AEC: 0x24420005},
    "bottom-left-uv-bias": {0x80105AFC: 0x2463FFD1},
    "bottom-right-uv-five": {0x80105B28: 0x24420005},
    "invert-dimming": {0x80105B2C: 0x16E00009},
    "dim-page-additive": {0x80105B34: 0x2402004C},
    "dim-brightness-31": {0x80105B3C: 0x2402001F},
    "dim-clut-wrong": {0x80105B50: 0x240237F6},
    "bright-page-dim": {0x80105B54: 0x2402002C},
    "bright-brightness-127": {0x80105B5C: 0x2402007F},
    "bright-clut-wrong": {0x80105B6C: 0x240237F5},
    "skip-clut-store": {0x80105B70: 0},
    "skip-primitive-link": {0x80105B88: 0},
    "digit-advance-four": {0x80105B94: 0x2631FFFC},
    "skip-cursor-advance": {0x80105BB0: 0},
}))
