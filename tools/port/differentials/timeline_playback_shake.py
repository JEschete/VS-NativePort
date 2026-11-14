"""F01588 func_800D6A18 (BATTLE.PRG 5BF94.c:8422), typed camera shake track tick."""

from timeline_playback_differential import SHAKE, configure

globals().update(
    configure(
        1588,
        SHAKE,
        0x800D6A18,
        0x800D6AEC,
        {
            "due-8-bits": {0x800D6A48: 0x304300FF},
            "due-test-inverted": {0x800D6A58: 0x10640014},
            "timer-6-bits": {0x800D6A6C: 0x3042003F},
            "held-flag-bit-1": {0x800D6A84: 0x30420002},
            "channels-from-timing": {0x800D6A60: 0x8CA70000},
            "timer-minus-2": {0x800D6AB4: 0x2442FFFE},
        },
    )
)
