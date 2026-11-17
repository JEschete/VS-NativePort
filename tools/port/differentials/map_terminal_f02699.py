"""F02699 _darkenBackground, MENU5.PRG 4D8.c:1788; typed map::DarkenBackground."""

from map_terminal_common import DARKEN, configure

globals().update(configure(2699, DARKEN, 0x8010625C, 0x80106570, {
    "subtract-page": {0x80106264: 0x24060040},
    "left-not-raw": {0x801062E8: 0x24020066},
    "left-width": {0x801062F0: 0x240200BF},
    "left-clut-not-cleared": {0x80106310: 0},
    "right-x": {0x80106430: 0x240200BF},
    "right-width": {0x80106438: 0x2402007F},
    "front-right-page": {0x801064F4: 0x24060102},
    "back-left-page": {0x8010633C: 0x24060104},
}))
