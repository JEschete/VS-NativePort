"""F02828 _printFastestClearTime, MENU9.PRG 238.c:2400."""

from gazette_times_common import FORMAT, configure

globals().update(configure(2828, FORMAT, 0x80106DE0, 0x80106F9C, {
    "ignore-no-clear": {0x80106DFC: 0x10000009},
    "cap-at-99-hours": {0x80106E44: 0x2A220063},
    "cap-hours-98": {0x80106E50: 0x24110062},
    "cap-minutes-58": {0x80106E58: 0x2412003A},
    "round-prefix-Q": {0x80106E68: 0x2402001A},
    "round-not-incremented": {0x80106E84: 0x24E60000},
    "round-glyph-bias": {0x80106EA8: 0x2463FFD1},
    "spacing-code": {0x80106EC0: 0x240200F9},
    "spacing-11": {0x80106ECC: 0x2402000B},
    "hours-glyph-bias": {0x80106F0C: 0x2463FFD1},
    "colon-code": {0x80106F24: 0x2402009D},
    "minutes-glyph-bias": {0x80106F64: 0x2463FFD1},
    "terminator-code": {0x80106F7C: 0x240200E6},
    "division-sign-correction": {0x80106E40: 0x00408821},
}))
