"""F02685 _configureMapMenu (MENU5.PRG 4D8.c:129), typed Menu::map::ConfigureAreaList."""

from map_loading_common import CONFIGURE, configure

globals().update(
    configure(
        2685,
        CONFIGURE,
        0x80102D90,
        0x80102ED8,
        {
            "current-not-remembered": {0x80102DD8: 0x00000000},
            "no-current-flag-ignored": {0x80102E00: 0x00000000},
            "mark-bit-two": {0x80102E2C: 0x34420002},
            "mark-row-limit-unsigned": {0x80102E14: 0x0068102B},
            "scroll-threshold-eight": {0x80102E04: 0x29020008, 0x80102E34: 0x29020008},
            "first-rows-threshold-nine": {0x80102E40: 0x28A20009},
            "first-row-not-cleared": {0x80102E5C: 0x00000000},
            "last-page-offset": {0x80102E6C: 0x2504FFF8},
            "centered-row-three": {0x80102E84: 0x24030003},
            "centered-first-row": {0x80102E90: 0x24A3FFFD},
            "row-info-zero": {0x80102EAC: 0x24050000},
            "memory-not-forced": {0x80102EB4: 0x00000000},
            "memory-not-restored": {0x80102EB8: 0x00000000},
        },
    )
)
