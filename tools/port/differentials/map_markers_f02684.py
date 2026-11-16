"""F02684 _renderDefaultIcon (MENU5.PRG 4D8.c:106), typed Menu::map::DrawAshleyMarker."""

from map_markers_common import ASHLEY, configure

globals().update(
    configure(
        2684,
        ASHLEY,
        0x80102D1C,
        0x80102D90,
        {
            "overlay-target-inverted": {0x80102D30: 0x10E00003},
            "brightness-64": {0x80102D4C: 0x24040040},
            "icon-base-5": {0x80102D58: 0x26030005},
            "command-66": {0x80102D6C: 0x3C026600},
            "colour-word-misplaced": {0x80102D78: 0xAC62FFF4},
        },
    )
)
