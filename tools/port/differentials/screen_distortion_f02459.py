"""F02459 func_800F9DE8 (GIM/SCREFF2.PRG 5E8.c:8), typed Effects::ScreenDistortion::SetVignetteEnabled."""

from screen_distortion_common import SET_VIGNETTE, configure

globals().update(
    configure(
        2459,
        SET_VIGNETTE,
        0x800F9DE8,
        0x800F9DF8,
        {
            "stores-halfword": {0x800F9DF4: 0xA4440008},
            "stores-at-4": {0x800F9DF4: 0xAC440004},
        },
    )
)
