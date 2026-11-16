"""F04159 func_80102E9C (MENUE.PRG 494.c:132, unused), typed Menu::manual::ClearMenuExceptRow8."""

from manual_screen_common import CLEAR, configure

globals().update(
    configure(
        4159,
        CLEAR,
        0x80102E9C,
        0x80102EBC,
        {
            "kept-row-9": {0x80102EA8: 0x24040009},
            "kept-row-0": {0x80102EA8: 0x24040000},
            "no-clear": {0x80102EA4: 0x00000000},
            "polls-instead": {0x80102EA4: 0x0C03EA74},
        },
    )
)
