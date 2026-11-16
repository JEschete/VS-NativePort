"""F00725 actor status captions (func_80095258) against its complete original instructions, F00726 and AddPrim."""

from differentials.floating_placement_common import STATUS, configure

globals().update(
    configure(
        725,
        STATUS,
        0x80095258,
        0x800957D0,
        {
            "hold-start-seven": {0x80095314: 0x2C620007},
            "closing-twenty-eight": {0x80095348: 0x2C62001C},
            "bright-start-twenty-six": {0x80095350: 0x2C62001A},
            "marker-frame-sixteen": {0x800953AC: 0x2C420010},
            "marker-step-cap-three": {0x800953D0: 0x24060003},
            "centre-quarter-width": {0x80095468: 0x00021083},
            "removal-colour-one": {0x80095484: 0x24060001},
            "visible-right-edge": {0x800954EC: 0x2A82013F},
            "half-height-seven": {0x8009552C: 0x2523FFF9},
            "bright-bias": {0x80095590: 0x24420061},
            "held-palette": {0x8009573C: 0x2402373E},
        },
    )
)
