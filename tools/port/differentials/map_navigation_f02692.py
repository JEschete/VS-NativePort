"""F02692 func_8010467C (MENU5.PRG 4D8.c:1048), typed Menu::map::MarkAllRoomsVisited."""

from map_navigation_common import MARK, configure

globals().update(
    configure(
        2692,
        MARK,
        0x8010467C,
        0x801046B0,
        {
            "visited-two": {0x80104690: 0x24060002},
            "record-stride-eight": {0x801046A4: 0x24A50008},
            "first-room-only": {0x801046A0: 0x00000000},
            "count-unchecked": {0x80104688: 0x00000000},
        },
    )
)
