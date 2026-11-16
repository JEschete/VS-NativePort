"""F04160 _menuReady (MENUE.PRG 494.c:134, unused), typed Menu::manual::PollMenuReady."""

from manual_screen_common import READY, configure

globals().update(
    configure(
        4160,
        READY,
        0x80102EBC,
        0x80102EDC,
        {
            "no-poll": {0x80102EC4: 0x00000000},
            "clears-instead": {0x80102EC4: 0x0C03EA38},
            "fades-instead": {0x80102EC4: 0x0C03FEDA},
        },
    )
)
