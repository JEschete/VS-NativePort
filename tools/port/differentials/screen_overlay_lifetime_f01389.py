"""F01389 func_800CB708 (BATTLE/BATTLE.PRG 5BF94.c:3896), typed Effects::ScreenDistortion::DrawScreenOverlay."""

from screen_overlay_lifetime_common import DRAW, configure

globals().update(
    configure(
        1389,
        DRAW,
        0x800CB708,
        0x800CB79C,
        {
            "neutral-scale-0": {0x800CB730: 0x24020000},
            "layer-low-byte": {0x800CB75C: 0x92020002},
            "loading-from-pages": {0x800CB724: 0x92020001},
        },
    )
)
