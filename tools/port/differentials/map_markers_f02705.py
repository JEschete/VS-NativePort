"""F02705 _renderIcon (MENU5.PRG 4D8.c:2158), typed Menu::map::DrawFeatureMarker."""

from map_markers_common import FEATURE, configure

globals().update(
    configure(
        2705,
        FEATURE,
        0x801074AC,
        0x80107630,
        {
            "length-8": {0x801074B8: 0x24020008},
            "half-width-quarter": {0x801074FC: 0x00021083},
            "half-rounding-lost": {0x801074F4: 0x00001821},
            "height-7": {0x80107534: 0x24C60007},
            "clut-37f6": {0x80107600: 0x240237F6},
            "tpage-0d": {0x801075F8: 0x2402000D},
            "slot-8": {0x80107614: 0x24840020},
        },
    )
)
