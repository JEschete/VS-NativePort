"""F04152 _renderMap (MENU5.PRG 4D8.c:1081), typed Menu::map::DrawVisitedRooms."""

from map_drawing_common import VISITED, configure

globals().update(
    configure(
        4152,
        VISITED,
        0x801046B0,
        0x80104780,
        {
            "centre-x-0x80": {0x801046E4: 0x24420080},
            "unvisited-drawn": {0x80104724: 0x00000000},
            "label-count-kept": {0x80104704: 0x00000000},
            "map-id-from-zone": {0x80104734: 0x86070004},
            "previous-offset-dropped": {0x80104760: 0x27A40018},
        },
    )
)
