"""F00741 timing-cue packet words and signed screen boundaries against original."""

from differentials.floating_label_rendering_support import specification

globals().update(
    specification(
        "F00741",
        0x800970BC,
        0x800971D4,
        5,
        {
            "left-radius-ten": {0x800970F0: 0x2442FFF6},
            "right-radius-ten": {0x80097110: 0x2442000A},
            "cue-height-thirty-one": {0x80097100: 0x2442FFE1},
            "cue-red-channel": {0x80097168: 0x240200F1},
            "cue-page": {0x80097178: 0x2402000C},
            "cue-palette": {0x80097180: 0x2402377D},
            "cue-texture-right-edge": {0x8009715C: 0x240400D5},
            "wrong-ot-bucket": {0x800971B8: 0x2484FFF0},
        },
    )
)
