"""F02708 _insertLine (MENU5.PRG 4D8.c:2258), typed Menu::map::AppendLabelLine."""

from map_markers_common import LINE, configure

globals().update(
    configure(
        2708,
        LINE,
        0x80107A9C,
        0x80107B10,
        {
            "length-4": {0x80107AB4: 0x24020004},
            "code-42": {0x80107ABC: 0x24020042},
            "grey-7f": {0x80107AC4: 0x2402007F},
            "y1-from-x1": {0x80107AD8: 0xA606000E},
            "slot-7": {0x80107AF4: 0x2484001C},
            "cursor-20": {0x80107AEC: 0x26100014},
        },
    )
)
