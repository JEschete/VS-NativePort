"""F01560 func_800D5780 (BATTLE.PRG 5BF94.c:7933), typed timeline key advance."""

from timeline_tracks_differential import ADVANCE, configure

globals().update(
    configure(
        1560,
        ADVANCE,
        0x800D5780,
        0x800D57FC,
        {
            "index-not-incremented": {0x800D5790: 0x24420000},
            "particle-kind-2": {0x800D57B4: 0x24020002},
            "shake-kind-12": {0x800D57BC: 0x2402000C},
            "end-returns-1": {0x800D57A8: 0x24020001},
            "index-stored-unincremented": {0x800D57B0: 0xA4830006},
        },
    )
)
