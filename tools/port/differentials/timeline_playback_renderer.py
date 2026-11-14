"""F01564 func_800D5D74 (BATTLE.PRG 5BF94.c:8069), typed renderer track tick."""

from timeline_playback_differential import RENDERER, configure

globals().update(
    configure(
        1564,
        RENDERER,
        0x800D5D74,
        0x800D5E00,
        {
            "due-8-bits": {0x800D5DA0: 0x30C200FF},
            "renderer-2-bits": {0x800D5DB8: 0x30A5000C},
            "parameter-5-bits": {0x800D5DCC: 0x30421F00},
            "parameter-or": {0x800D5DD0: 0x00621825},
            "key-stride-4": {0x800D5D8C: 0x00021080},
        },
    )
)
