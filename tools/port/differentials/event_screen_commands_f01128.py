"""F01128 func_800B9C58 (BATTLE.PRG 4A0A8.c:2122), typed Event::ScriptTransformPalettes."""

from event_screen_commands_common import TRANSFORM_PALETTES, configure

globals().update(
    configure(
        1128,
        TRANSFORM_PALETTES,
        0x800B9C58,
        0x800B9D74,
        {
            "background-mode-byte": {0x800B9C9C: 0x308400FF},
            "actor-id-unmasked": {0x800B9CC8: 0x00403021},
            "scene-duration-six-bits": {0x800B9D28: 0x30A5003F},
            "actor-red-unsigned": {0x800B9CDC: 0x92070005},
        },
    )
)
