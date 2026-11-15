"""F02462 func_800F9FB8 (GIM/SCREFF2.PRG 5E8.c:41), typed Effects::ScreenDistortion::DrawVignette."""

from screen_distortion_common import DRAW_VIGNETTE, configure

globals().update(
    configure(
        2462,
        DRAW_VIGNETTE,
        0x800F9FB8,
        0x800FA114,
        {
            "first-corner-step-7": {0x800F9FC4: 0x24050007},
            "rim-tag-length-8": {0x800FA06C: 0x3C020800},
            "inner-radius-0x61": {0x800FA0A4: 0x24050061},
        },
    )
)

VECTORS, MUTANT_VECTORS = 96, 24
