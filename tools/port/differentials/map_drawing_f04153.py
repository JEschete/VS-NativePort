"""F04153 _renderRoom (MENU5.PRG 4D8.c:1112), typed Menu::map::DrawRoom."""

from map_drawing_common import ROOM, configure

globals().update(
    configure(
        4153,
        ROOM,
        0x80104780,
        0x80105B18,
        {
            "depth-span-0x7c0": {0x801047FC: 0x2C4207C0},
            "triangle-code-0x32": {0x8010490C: 0x24020032},
            "quad-code-0x3a": {0x80104B94: 0x2402003A},
            "glow-opaque": {0x80104EAC: 0x24090050},
            "door-icon-rise-13": {0x8010566C: 0x24C6FFF3},
            "workshop-kinds-short": {0x80105828: 0x24840002},
            "kind-period-16": {0x8010586C: 0x00051103},
            "ashley-inset-7": {0x80105914: 0x2508FFF9},
        },
    )
)
