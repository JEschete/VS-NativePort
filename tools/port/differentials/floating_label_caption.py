"""F00723 value-caption and sign packets against their complete original instructions."""

from differentials.floating_label_rendering_support import specification

globals().update(
    specification(
        "F00723",
        0x80094E18,
        0x8009506C,
        0,
        {
            "caption-kind-minus-four": {0x80094E68: 0x2442FFFC},
            "caption-gap-ten": {0x80094EB0: 0x2652000A},
            "caption-vertical-radius-five": {0x80094E90: 0x2683FFFB},
            "caption-translucent": {0x80094E40: 0x2416002E},
            "sign-bit-fourteen": {0x80094F30: 0x32734000},
            "caption-palette": {0x80094F74: 0x2402373E},
            "caption-page": {0x80094F6C: 0x2402000C},
            "sign-gap-three": {0x80094FDC: 0x27C20003},
            "caption-wrong-ot-bucket": {0x80094F90: 0x2484FFF0},
            "sign-not-linked": {0x8009502C: 0x00000000},
        },
    )
)
