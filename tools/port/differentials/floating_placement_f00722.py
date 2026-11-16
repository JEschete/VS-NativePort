"""F00722 actor value digits (func_80094B0C) against its complete original instructions, F00723 and AddPrim."""

from differentials.floating_placement_common import VALUE, configure

globals().update(
    configure(
        722,
        VALUE,
        0x80094B0C,
        0x80094E18,
        {
            "bounce-clamp-fourteen": {0x80094B28: 0x2406000E},
            "rise-reversed": {0x80094C24: 0x02A2A821},
            "negative-bounce-shift-three": {0x80094CA0: 0x80520003},
            "offset-quartered": {0x80094CE0: 0x00029083},
            "visible-span-narrow": {0x80094CE8: 0x2C820147},
            "digit-palette": {0x80094DA8: 0x240237F5},
            "digit-advance-five": {0x80094DDC: 0x2673FFFB},
        },
    )
)
