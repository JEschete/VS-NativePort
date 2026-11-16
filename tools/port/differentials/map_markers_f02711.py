"""F02711 _renderConnectingMapName (MENU5.PRG 4D8.c:2403), typed Menu::map::DrawConnectingAreaLabel."""

from map_markers_common import AREA_LABEL, configure

globals().update(
    configure(
        2711,
        AREA_LABEL,
        0x8010815C,
        0x80108274,
        {
            "pivot-186": {0x8010816C: 0x240200BA},
            "pivot-test-inverted": {0x8010818C: 0x06000002},
            "flag-word-shift-6": {0x801081D0: 0x00111983},
            "flag-bit-mask-f": {0x801081DC: 0x3224000F},
            "visited-inverted": {0x801081F0: 0x10600002},
            "unvisited-name-33": {0x801081F8: 0x24110021},
            "row-x-194": {0x80108200: 0x240500C2},
            "background-16": {0x80108234: 0x24030010},
        },
    )
)
