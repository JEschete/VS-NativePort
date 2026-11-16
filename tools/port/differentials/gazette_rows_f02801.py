"""F02801 _getSelectedRow (MENU9.PRG 238.c:228), typed Menu::Score::GetSelectedTopMenuRow."""

from gazette_rows_common import SELECTED, configure

globals().update(
    configure(
        2801,
        SELECTED,
        0x80102F0C,
        0x80102F64,
        {
            "kept-row-4": {0x80102F30: 0x24040004},
            "cancel-below-minus-2": {0x80102F20: 0x2A02FFFE},
            "hoist-offset-0": {0x80102F4C: 0x24050000},
            "first-row-hoisted": {0x80102F44: 0x90444EEB},
            "pending-flies-out": {0x80102F3C: 0x00000000},
        },
    )
)
