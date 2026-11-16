"""F02710 _computeRotatedRect (MENU5.PRG 4D8.c:2377), typed Menu::map::ComputeCompassCorners."""

from map_drawing_common import CORNERS, configure

globals().update(
    configure(
        2710,
        CORNERS,
        0x8010800C,
        0x8010815C,
        {
            "sine-cosine-swapped": {0x8010803C: 0x0C0103C9, 0x80108048: 0x0C0103CA},
            "round-negative-dropped": {0x801080F4: 0x00000000},
            "shift-12": {0x801080FC: 0x00021303},
            "centre-y-from-x": {0x80108124: 0x00731821},
            "corner-unnegated": {0x801080B8: 0x00000000},
        },
    )
)
