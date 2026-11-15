"""F01431 func_800CEF74 (BATTLE.PRG 5BF94.c:5399), typed effect track start."""

from effect_runtime_differential import START_TRACK, configure

globals().update(
    configure(
        1431,
        START_TRACK,
        0x800CEF74,
        0x800CF060,
        {
            "select-secondary": {0x800CEF80: 0x24040000},
            "script-offset-double": {0x800CEFD4: 0x00000000},
            "token-dropped": {0x800CF008: 0x00000000},
            "phase-5": {0x800CF018: 0x24040005},
            "started-kept": {0x800CF02C: 0x00000000},
            "pending-kept": {0x800CF03C: 0x00000000},
        },
    )
)
