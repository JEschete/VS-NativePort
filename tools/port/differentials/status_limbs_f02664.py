"""F02664 original status icons with actual flag and sprite helper instructions."""

from status_limbs_common import configure

globals().update(
    configure(
        2664,
        {
            "four-columns": {0x80104044: 0x32640003},
            "row-pitch-eight": {0x8010405C: 0x000318C0},
            "row-top-143": {0x80104060: 0x2463008F},
            "base-x-15": {0x80104054: 0x26E2FFF1},
            "element-threshold-17": {0x80104064: 0x2A020011},
            "element-offset-seven": {0x80104074: 0x26450007},
            "element-uv": {0x8010409C: 0x24633069},
            "element-clut": {0x801040A0: 0x3C0437FE},
            "icon-width-15": {0x801040C0: 0x34C6000F},
            "palette-mask-bit-zero": {0x801040D0: 0x3463906B},
            "signed-uv-load": {0x801040E0: 0x86850000},
            "skip-last-status": {0x801040FC: 0x2A02001F},
        },
    )
)
