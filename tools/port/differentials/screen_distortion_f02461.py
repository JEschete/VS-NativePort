"""F02461 func_800F9EBC (GIM/SCREFF2.PRG 5E8.c:23), typed Effects::ScreenDistortion::AppendVignetteCorner."""

from screen_distortion_common import CORNER, configure

globals().update(
    configure(
        2461,
        CORNER,
        0x800F9EBC,
        0x800F9FB8,
        {
            "radius-0x7f": {0x800F9F28: 0x2405007F},
            "three-triangles": {0x800F9F78: 0x26C20003},
            "tag-length-7": {0x800F9F30: 0x3C020700},
        },
    )
)
