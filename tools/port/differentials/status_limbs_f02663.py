"""F02663 original leader: signed anchors, geometry, opacity and both tags."""

from status_limbs_common import configure

globals().update(
    configure(
        2663,
        {
            "leader-color": {0x80103F00: 0x34637019},
            "row-pitch-eight": {0x80103F18: 0x000420C0},
            "row-top-43": {0x80103F1C: 0x248A002B},
            "unsigned-anchor-y": {0x80103F20: 0x00091402},
            "elbow-distance-doubled": {0x80103F60: 0x00041080},
            "no-absolute-delta": {0x80103F44: 0x00000000},
            "no-half-distance": {0x80103F50: 0x00402021},
            "first-tag-seven": {0x80103F78: 0x3C030700},
            "last-x-217": {0x80103FB4: 0x348400D9},
            "second-tag-four": {0x80103FBC: 0x3C020400},
            "short-packet-advance": {0x80103FD8: 0x24C60020},
        },
    )
)
