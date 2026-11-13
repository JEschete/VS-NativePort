"""F00726 removal-marker signed frame wrapping and exact packet-word oracle."""

from differentials.floating_label_rendering_support import specification

globals().update(
    specification(
        "F00726",
        0x800957D0,
        0x800958A4,
        2,
        {
            "left-radius-seven": {0x800957E0: 0x2484FFF9},
            "width-fifteen": {0x800957E4: 0x2483000F},
            "frame-stride-eight": {0x800957F4: 0x000630C0},
            "texture-u-base-sixty-five": {0x80095814: 0x24C30041},
            "texture-v-forty-nine": {0x80095820: 0x24020031},
            "marker-palette": {0x80095850: 0x2403373D},
            "modulated-instead-of-raw": {0x80095874: 0x34420000},
            "wrong-ot-bucket": {0x80095888: 0x2484FFF0},
        },
    )
)
