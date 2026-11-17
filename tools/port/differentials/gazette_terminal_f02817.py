"""F02817 _renderEnemyNo, MENU9.PRG 238.c:1878."""

from gazette_terminal_common import NUMBER, configure

globals().update(configure(2817, NUMBER, 0x80105BCC, 0x80105D8C, {
    "right-offset-twenty-three": {0x80105C10: 0x26310017},
    "omit-hundreds": {0x80105C14: 0x24120001},
    "raise-three": {0x80105C18: 0x26D4FFFD},
    "height-nine": {0x80105C24: 0x24150009},
    "wrong-packet-length": {0x80105C2C: 0x24020008},
    "width-five": {0x80105C38: 0x26240005},
    "opaque-digits": {0x80105C44: 0x34420000},
    "raw-digits": {0x80105C48: 0x34420001},
    "units-uv-biased": {0x80105C80: 0x2463FFD1},
    "invert-dimming": {0x80105CEC: 0x16E00009},
    "dim-page-additive": {0x80105CF4: 0x2402004C},
    "dim-brightness-31": {0x80105CFC: 0x2402001F},
    "dim-clut-wrong": {0x80105D10: 0x240237F6},
    "bright-page-dim": {0x80105D14: 0x2402002C},
    "bright-brightness-127": {0x80105D1C: 0x2402007F},
    "bright-clut-wrong": {0x80105D2C: 0x240237F5},
    "scene-instead-of-overlay-ordering": {0x80105D44: 0x8C840004},
    "omit-primitive-link": {0x80105D48: 0},
    "digit-advance-four": {0x80105D54: 0x2631FFFC},
    "omit-primitive-cursor": {0x80105D70: 0},
}))
