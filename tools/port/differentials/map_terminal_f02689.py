"""F02689 _renderScreen, MENU5.PRG 4D8.c:905; typed map::DrawFrame."""

from map_terminal_common import FRAME, configure

globals().update(configure(2689, FRAME, 0x801042B0, 0x80104384, {
    "mask-disabled": {0x801042CC: 0x24050000},
    "mask-wrong-slot": {0x801042E8: 0x24841FF8},
    "no-controls": {0x801042BC: 0},
    "no-caption": {0x80104330: 0},
    "no-camera": {0x80104340: 0},
    "no-darkening": {0x80104348: 0},
    "no-rooms": {0x80104368: 0},
    "caption-offset-bytes": {0x8010432C: 0},
}))
