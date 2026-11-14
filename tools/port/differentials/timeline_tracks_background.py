"""F01570 func_800D6310 (BATTLE.PRG 5BF94.c:8201), typed background colour track."""

from timeline_tracks_differential import BACKGROUND, configure

globals().update(
    configure(
        1570,
        BACKGROUND,
        0x800D6310,
        0x800D6388,
        {
            "blue-not-centred": {0x800D635C: 0x24420000},
            "mode-byte-4": {0x800D6348: 0x90E40004},
            "key-stride-4": {0x800D6328: 0x00021080},
            "waiting-returns-0": {0x800D6344: 0x24020000},
        },
    )
)
