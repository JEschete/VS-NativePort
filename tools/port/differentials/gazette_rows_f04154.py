"""F04154 _clearMenuExceptRow8 (MENU9.PRG 238.c:258, unused), typed Menu::Score::ClearMenuExceptRow8."""

from gazette_rows_common import CLEAR_ROW_8, configure

globals().update(
    configure(
        4154,
        CLEAR_ROW_8,
        0x80102FB8,
        0x80102FD8,
        {
            "kept-row-9": {0x80102FC4: 0x24040009},
            "kept-row-0": {0x80102FC4: 0x24040000},
            "no-clear": {0x80102FC0: 0x00000000},
            "flies-out-instead": {0x80102FC0: 0x0C03EA4B},
            "polls-instead": {0x80102FC0: 0x0C03EA74},
        },
    )
)
