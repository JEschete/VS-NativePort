"""F02714 func_8010839C (MENU5.PRG 4D8.c:2471), typed Menu::map::ShouldDrawDoorStateMarker."""

from map_markers_common import SHOULD, configure

globals().update(
    configure(
        2714,
        SHOULD,
        0x8010839C,
        0x801083F8,
        {
            "sigil-base-40": {0x801083B0: 0x2606FFC0},
            "sigil-kinds-8": {0x801083B4: 0x2CC20008},
            "unlocked-inverted": {0x801083C8: 0x10400007},
            "hidden-from-1": {0x801083E4: 0x2C420001},
            "unlocked-drawn": {0x801083CC: 0x24020001},
        },
    )
)
