"""F02686 _mapSelection (MENU5.PRG 4D8.c:175), typed Menu::map::PollAreaSelection."""

from map_loading_common import POLL, configure

globals().update(
    configure(
        2686,
        POLL,
        0x80102ED8,
        0x80102F30,
        {
            "cancel-threshold-zero": {0x80102EEC: 0x2A020000},
            "kept-row-four": {0x80102EFC: 0x24040004},
            "cancel-result-pending": {0x80102F04: 0x2402FFFF},
            "chosen-not-flown": {0x80102F14: 0x00000000},
            "hoist-offset-zero": {0x80102F18: 0x24050000},
        },
    )
)
