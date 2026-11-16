"""F04147 _menuReady (MENU5.PRG 4D8.c:204), typed Menu::map::PollMenuReady."""

from map_loading_common import READY, configure

globals().update(
    configure(
        4147,
        READY,
        0x80102FAC,
        0x80102FCC,
        {
            "ready-not-polled": {0x80102FB4: 0x00000000},
        },
    )
)
