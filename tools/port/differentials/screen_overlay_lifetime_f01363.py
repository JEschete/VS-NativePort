"""F01363 func_800CA97C (BATTLE/BATTLE.PRG 5BF94.c:3477), typed Effects::ScreenDistortion::ResetScreenOverlay."""

from screen_overlay_lifetime_common import RESET, configure

globals().update(
    configure(
        1363,
        RESET,
        0x800CA97C,
        0x800CA9C0,
        {
            "memset-12": {0x800CA998: 0x2406000C},
            "scale-0x1001": {0x800CA9A8: 0x24021001},
            "keeps-pointer": {0x800CA9A4: 0x00000000},
        },
    )
)
