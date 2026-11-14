"""F01569 func_800D6298 (BATTLE.PRG 5BF94.c:8189), typed scene colour track."""

from timeline_tracks_differential import SCENE, configure

globals().update(
    configure(
        1569,
        SCENE,
        0x800D6298,
        0x800D6310,
        {
            "red-from-green": {0x800D62D8: 0x90E60005},
            "mode-byte-6": {0x800D62D0: 0x90E40006},
            "duration-unsigned": {0x800D62D4: 0x94E50002},
            "frame-unsigned": {0x800D62BC: 0x94E30000},
        },
    )
)
