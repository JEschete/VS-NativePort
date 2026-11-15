"""F01124 func_800B9AE8 (BATTLE.PRG 4A0A8.c:2073), typed Event::ScriptConfigureScreenTransition."""

from event_screen_commands_common import CONFIGURE, configure

globals().update(
    configure(
        1124,
        CONFIGURE,
        0x800B9AE8,
        0x800B9B18,
        {
            "speed-signed": {0x800B9AFC: 0x80450002},
            "angle-reads-first": {0x800B9AF4: 0x90460001},
        },
    )
)
