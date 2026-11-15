"""F01121 func_800B9A40 (BATTLE.PRG 4A0A8.c:2049), typed Event::ScriptFadeOutAndWait."""

from event_screen_commands_common import FADE_OUT, configure

globals().update(
    configure(
        1121,
        FADE_OUT,
        0x800B9A40,
        0x800B9A94,
        {
            "waits-for-phase-6": {0x800B9A58: 0x24020006},
            "idle-advances": {0x800B9A68: 0x24020000},
            "speed-reads-third": {0x800B9A74: 0x92050003},
            "angle-times-eight": {0x800B9A7C: 0x000630C0},
        },
    )
)
