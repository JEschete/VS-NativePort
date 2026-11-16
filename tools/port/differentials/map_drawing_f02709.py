"""F02709 _renderCompass (MENU5.PRG 4D8.c:2272), typed Menu::map::DrawCompass."""

from map_drawing_common import COMPASS, configure

globals().update(
    configure(
        2709,
        COMPASS,
        0x80107B10,
        0x8010800C,
        {
            "tick-turn-128": {0x80107BA0: 0x001211C0},
            "all-ticks-major": {0x80107BC8: 0x00000000},
            "north-clut-shared": {0x80107DA8: 0x2402373F},
            "glow-near-5": {0x80107DE4: 0x2442FFFB},
            "hub-quarter-dropped": {0x80107EEC: 0x24420000},
            "needle-length-15": {0x80107EFC: 0x2407000F},
            "needle-slot-0x1c": {0x80107FD4: 0x2484001C},
        },
    )
)
