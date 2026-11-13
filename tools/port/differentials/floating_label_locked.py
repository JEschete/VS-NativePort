"""F00724 four-piece locked-label geometry, clipping and OT traversal oracle."""

from differentials.floating_label_rendering_support import specification

globals().update(
    specification(
        "F00724",
        0x8009506C,
        0x80095258,
        1,
        {
            "opening-threshold-two": {0x80095098: 0x2C620002},
            "closing-threshold-twenty-nine": {0x800950AC: 0x2C62001D},
            "last-frame-twenty-nine": {0x800950B4: 0x2402001D},
            "left-margin-fifteen": {0x800950C8: 0x2652FFF1},
            "right-bound-three-nineteen": {0x80095100: 0x2A42013F},
            "left-bound-exclusive": {0x80095118: 0x1840003C},
            "height-add-instead-of-subtract": {0x800950F4: 0x00A2A821},
            "modulated-instead-of-raw": {0x80095158: 0x34420000},
            "opaque-instead-of-translucent": {0x800951DC: 0x34630000},
            "piece-texture-height-eleven": {0x800951B4: 0x2442000B},
            "wrong-piece-palette": {0x800951F0: 0x240237F5},
        },
    )
)
