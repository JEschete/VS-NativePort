"""F01562 func_800D5904 (BATTLE.PRG 5BF94.c:7974), typed sound track tick."""

from timeline_playback_differential import SOUND, configure

globals().update(
    configure(
        1562,
        SOUND,
        0x800D5904,
        0x800D5A98,
        {
            "due-8-bits": {0x800D5934: 0x30A200FF},
            "shared-bank-7f": {0x800D5984: 0x2404007F},
            "bank-1-bit": {0x800D598C: 0x30420001},
            "translation-x-for-y": {0x800D5A0C: 0x94420068},
            "zero-stream-plays": {0x800D5964: 0x00000000},
            "stream-source-3": {0x800D5948: 0x24020003},
        },
    )
)
