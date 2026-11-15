"""F02460 func_800F9DF8 (GIM/SCREFF2.PRG 5E8.c:10), typed Effects::ScreenDistortion::EllipsePoint."""

from screen_distortion_common import ELLIPSE, configure

globals().update(
    configure(
        2460,
        ELLIPSE,
        0x800F9DF8,
        0x800F9EBC,
        {
            "centre-x-161": {0x800F9E94: 0x246300A1},
            "widened-shift-14": {0x800F9E6C: 0x00021383},
            "scallop-base-63": {0x800F9E54: 0x2631003F},
        },
    )
)
