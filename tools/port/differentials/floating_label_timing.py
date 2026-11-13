"""F00727 original timing phrases through the actual compact-font instructions."""

from differentials.floating_label_rendering_support import specification

globals().update(
    specification(
        "F00727",
        0x800958A4,
        0x80095A4C,
        3,
        {
            "opening-threshold-three": {0x80095988: 0x2C620003},
            "closing-threshold-forty-one": {0x8009599C: 0x2C620029},
            "last-frame-forty-four": {0x800959A4: 0x2402002C},
            "success-value-one": {0x800959CC: 0x24020001},
            "slide-adds-instead": {0x800959C4: 0x00822021},
            "success-red-channel": {0x80095A18: 0x34C68041},
            "success-index-uses-mode": {0x80095A1C: 0x90E2000E},
            "timing-table-stride-two": {0x800959EC: 0x00021040},
            "wrong-text-ot-bucket": {0x80095A38: 0x24E7FFEC},
        },
    )
)
