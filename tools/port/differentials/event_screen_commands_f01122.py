"""F01122 func_800B9A94 (BATTLE.PRG 4A0A8.c:2061), typed Event::ScriptFadeIn."""

from event_screen_commands_common import FADE_IN, configure

globals().update(
    configure(
        1122,
        FADE_IN,
        0x800B9A94,
        0x800B9AC4,
        {
            "effect-signed": {0x800B9AA4: 0x80440001},
            "angle-times-32": {0x800B9AB0: 0x00063140},
            "returns-hold": {0x800B9AB8: 0x24020001},
        },
    )
)
