"""F02800 _setRows (MENU9.PRG 238.c:196), typed Menu::Score::RestoreTopMenuCursor."""

from gazette_rows_common import SET_ROWS, configure

globals().update(
    configure(
        2800,
        SET_ROWS,
        0x80102E10,
        0x80102F0C,
        {
            "visible-rows-7": {0x80102E68: 0x29020008},
            "early-rows-9": {0x80102E74: 0x28A20009},
            "middle-row-5": {0x80102EBC: 0x24030005},
            "setting-kept-on": {0x80102EEC: 0x00000000},
            "row-info-0x102": {0x80102EE0: 0x24050102},
            "last-page-minus-8": {0x80102EA4: 0x2504FFF8},
        },
    )
)
