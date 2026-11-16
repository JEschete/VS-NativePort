"""F02693 _snapMapToRoom (MENU5.PRG 4D8.c:1511), typed Menu::map::CenterOnRoom."""

from map_navigation_common import CENTER_ON, configure

globals().update(
    configure(
        2693,
        CENTER_ON,
        0x80105B18,
        0x80105D3C,
        {
            "minimum-starts-7ffe": {0x80105B20: 0x24027FFE},
            "maximum-x-compare-swapped": {0x80105BE4: 0x0062102A},
            "x-rounding-lost": {0x80105C64: 0x00001821},
            "half-shift-two": {0x80105C6C: 0x00021083},
            "y-from-minimum": {0x80105C90: 0x97A30002},
            "translation-skipped": {0x80105C9C: 0x10000020},
            "x-added": {0x80105CD4: 0x00431021},
            "y-clear-skipped": {0x80105D30: 0x00000000},
        },
    )
)
