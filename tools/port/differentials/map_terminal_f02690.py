"""F02690 _setCamera, MENU5.PRG 4D8.c:948; typed map::ApplyCamera."""

from map_terminal_common import CAMERA, configure

globals().update(configure(2690, CAMERA, 0x80104384, 0x8010451C, {
    "x-round-negative-down": {0x801043E4: 0},
    "x-not-stored": {0x8010440C: 0},
    "z-distance-not-negated": {0x80104424: 0},
    "z-not-stored": {0x80104460: 0},
    "y-distance-not-negated": {0x8010446C: 0},
    "y-not-stored": {0x80104494: 0},
    "translation-x-not-negated": {0x801044C4: 0},
    "rotation-installs-pitch": {0x801044EC: 0x02202021},
}))
