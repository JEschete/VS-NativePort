"""F02687 _setMenuItemMapName (MENU5.PRG 4D8.c:188), typed Menu::map::AreaNameRowOf."""

from map_loading_common import NAME, configure

globals().update(
    configure(
        2687,
        NAME,
        0x80102F30,
        0x80102F8C,
        {
            "background-width-nine": {0x80102F3C: 0x24020009},
            "row-nine": {0x80102F44: 0x24040009},
            "state-three": {0x80102F60: 0x24030003},
            "target-x": {0x80102F68: 0x240300B5},
            "not-selected": {0x80102F74: 0x00000000},
            "font-color-dropped": {0x80102F78: 0x00000000},
        },
    )
)
