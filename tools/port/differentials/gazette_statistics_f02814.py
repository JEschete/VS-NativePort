"""F02814 _setTitleRows, MENU9.PRG 238.c:1722."""

from gazette_statistics_common import CREATE_TITLES, configure

globals().update(configure(2814, CREATE_TITLES, 0x801056B8, 0x8010579C, {
    "title-x-59": {0x801056C4: 0x240F003B},
    "flag-starts-at-bit-one": {0x801056CC: 0x240A0002},
    "row-number-zero-based": {0x801056F4: 0x24A20000},
    "keep-selected": {0x80105700: 0},
    "keep-animation": {0x80105704: 0},
    "keep-disabled": {0x80105708: 0},
    "invert-unlocked": {0x80105718: 0x1440000C},
    "unlocked-highlight-off": {0x80105720: 0xA080FFF5},
    "title-offset-byte-stride": {0x8010572C: 0x00021000},
    "description-offset-byte-stride": {0x80105740: 0x00021000},
    "locked-highlight-on": {0x8010574C: 0xA08AFFF5},
    "wrong-locked-description": {0x80105764: 0x95220000},
    "only-31-titles": {0x80105788: 0x28A2001F},
}))
