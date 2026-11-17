"""F02818 _initEnemyList, MENU9.PRG 238.c:1916."""

from gazette_terminal_common import INITIALIZE, configure

globals().update(configure(2818, INITIALIZE, 0x80105D8C, 0x80105F00, {
    "do-not-clear-old-unlocked": {0x80105DC0: 0},
    "wrong-defeat-flag-word": {0x80105DE8: 0x8C6300A0},
    "do-not-set-unlocked": {0x80105E00: 0},
    "only-seventy-seven-defeat-ranges": {0x80105E14: 0x28A2004D},
    "wrong-record-stride": {0x80105E1C: 0x24C60028},
    "write-locked-previous-link": {0x80105E3C: 0x14400003},
    "skip-previous-link": {0x80105E44: 0},
    "skip-first-to-last-wrap": {0x80105E80: 0},
    "skip-next-link": {0x80105EB4: 0},
    "skip-last-to-first-wrap": {0x80105EEC: 0},
}))
