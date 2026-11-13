"""F00728 original sprintf and compact-font chain caption and slide oracle."""

from differentials.floating_label_rendering_support import specification

globals().update(
    specification(
        "F00728",
        0x80095A4C,
        0x80095B70,
        4,
        {
            "opening-threshold-three": {0x80095A94: 0x2C420003},
            "opening-slides-two-chains": {0x80095AA4: 0x24020002},
            "closing-threshold-eighty-eight": {0x80095AD4: 0x2C420058},
            "last-frame-eighty-nine": {0x80095ADC: 0x24020059},
            "singular-value-two": {0x80095B04: 0x24020002},
            "plural-uses-singular-format": {0x80095B30: 0x24A5931C},
            "chain-red-channel": {0x80095B50: 0x34C640A1},
            "wrong-text-ot-bucket": {0x80095B58: 0x24E7FFEC},
            "opening-slide-adds": {0x80095AC8: 0x02028021},
        },
    )
)
