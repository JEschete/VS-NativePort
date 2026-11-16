"""F02694 _setCenterpoint (MENU5.PRG 4D8.c:1578), typed Menu::map::RoomCenterOf."""

from map_navigation_common import SET_TARGET, configure

globals().update(
    configure(
        2694,
        SET_TARGET,
        0x80105D3C,
        0x80105EC0,
        {
            "minimum-starts-7ffe": {0x80105D40: 0x24027FFE},
            "maximum-starts-zero": {0x80105D50: 0x24020000},
            "record-stride-eight": {0x80105D64: 0x00000000},
            "x-half-shift-two": {0x80105E80: 0x00021083},
            "z-rounding-lost": {0x80105EA0: 0x00001821},
            "y-from-minimum": {0x80105EA8: 0x97A30002},
            "y-not-stored": {0x80105EB4: 0x00000000},
        },
    )
)
