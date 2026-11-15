"""F02455 func_800F9800 (GIM/SCREFF2.PRG 0.c:8), typed Effects::ScreenDistortion::SetFeedbackLayer."""

from screen_distortion_common import SET_LAYER, configure

globals().update(
    configure(
        2455,
        SET_LAYER,
        0x800F9800,
        0x800F986C,
        {
            "stores-low-byte": {0x800F981C: 0xA0640002},
            "lends-page-28": {0x800F982C: 0x2404001C},
            "retires-on-nonzero": {0x800F9844: 0x10800005},
        },
    )
)
