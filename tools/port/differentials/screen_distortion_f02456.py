"""F02456 func_800F986C (GIM/SCREFF2.PRG 0.c:22), typed Effects::ScreenDistortion::DrawFeedbackTiles."""

from screen_distortion_common import DRAW_TILES, configure

globals().update(
    configure(
        2456,
        DRAW_TILES,
        0x800F986C,
        0x800F9BC0,
        {
            "slot-layer-less-two": {0x800F9AD4: 0x2450FFFE},
            "blends-pages-0x130": {0x800F9A48: 0x34420130},
            "restores-x-0x100": {0x800F9B30: 0x24110100},
            "edge-height-5": {0x800F98E8: 0x3C060005},
        },
    )
)

# Each vector runs over 30,000 instructions; fewer vectors keep the case quick.
VECTORS, MUTANT_VECTORS = 48, 12
