"""F01573 func_800D6418 (BATTLE.PRG 5BF94.c:8233), typed target room tile."""

from timeline_commands_differential import ROOM_TILE, configure

globals().update(
    configure(
        1573,
        ROOM_TILE,
        0x800D6418,
        0x800D6448,
        {
            "tile-type-4": {0x800D6424: 0x24020004},
            "tile-reads-source": {0x800D6420: 0x94830D1C},
        },
    )
)
