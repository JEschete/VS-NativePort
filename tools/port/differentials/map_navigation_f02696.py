"""F02696 _scaleRoomVertices (MENU5.PRG 4D8.c:1728), typed Menu::map::ScaleRoomVertices."""

from map_navigation_common import SCALE, configure

globals().update(
    configure(
        2696,
        SCALE,
        0x801060E0,
        0x80106178,
        {
            "x-high-word": {0x8010611C: 0x00001010},
            "y-kept": {0x80106148: 0x00000000},
            "first-vertex-only": {0x80106158: 0x00000000},
            "first-room-only": {0x80106168: 0x00000000},
        },
    )
)
