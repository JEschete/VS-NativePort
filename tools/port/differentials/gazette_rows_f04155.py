"""F04155 _menuReady (MENU9.PRG 238.c:264, unused), typed Menu::Score::PollMenuReady."""

from gazette_rows_common import READY, configure

globals().update(
    configure(
        4155,
        READY,
        0x80102FD8,
        0x80102FF8,
        {
            "no-poll": {0x80102FE0: 0x00000000},
            "clears-instead": {0x80102FE0: 0x0C03EA38},
            "flies-out-instead": {0x80102FE0: 0x0C03EA4B},
            "selection-instead": {0x80102FE0: 0x0C040232},
        },
    )
)
