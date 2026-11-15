"""F01123 func_800B9AC4 (BATTLE.PRG 4A0A8.c:2067), typed Event::ScriptSetWipeAngle."""

from event_screen_commands_common import WIPE_ANGLE, configure

globals().update(
    configure(
        1123,
        WIPE_ANGLE,
        0x800B9AC4,
        0x800B9AE8,
        {
            "angle-signed": {0x800B9ACC: 0x80840001},
            "angle-times-32": {0x800B9AD4: 0x00042140},
        },
    )
)
