"""F02457 func_800F9BC0 (GIM/SCREFF2.PRG 0.c:114), typed Effects::ScreenDistortion::SetRadialWarp."""

from screen_distortion_common import SET_WARP, configure

globals().update(
    configure(
        2457,
        SET_WARP,
        0x800F9BC0,
        0x800F9BD8,
        {
            "growth-over-scale": {0x800F9BD4: 0xA4450004},
            "scale-low-byte": {0x800F9BCC: 0xA0440004},
        },
    )
)
