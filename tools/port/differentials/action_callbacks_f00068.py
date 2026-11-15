"""F00068 func_8006D028 (BATTLE.PRG 146C.c:2211), typed Effects::TintActionActorRed."""

from action_callbacks_common import TINT, configure

globals().update(
    configure(
        68,
        TINT,
        0x8006D028,
        0x8006D064,
        {
            "tint-mode-13": {0x8006D034: 0x2404000D},
            "duration-4": {0x8006D038: 0x24050004},
            "red-0x20": {0x8006D03C: 0x24070020},
            "tinted-not-stored": {0x8006D044: 0x00000000},
            "green-from-actor": {0x8006D048: 0xAFA60010},
        },
    )
)
