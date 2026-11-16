"""F02695 _smoothMapToCenterpoint (MENU5.PRG 4D8.c:1629), typed Menu::map::AnimateCentering."""

from map_navigation_common import ANIMATE, configure

globals().update(
    configure(
        2695,
        ANIMATE,
        0x80105EC0,
        0x801060E0,
        {
            "z-ignored-when-x-y-zero": {0x80105EF0: 0x10000079},
            "whole-step-below-seven": {0x80105F10: 0x28C20007},
            "fixed-step-up-to-127": {0x80105F18: 0x28C20080},
            "eighth-rounding-lost": {0x80105F34: 0x00A01021},
            "y-negative-step-seven": {0x80105FB0: 0x2402FFF9},
            "z-quarter-step": {0x80105FFC: 0x00021083},
            "x-added": {0x80106054: 0x00431021},
            "center-y-kept": {0x801060D0: 0x00000000},
        },
    )
)
