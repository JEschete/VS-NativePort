"""F01139 func_800BA218 (BATTLE.PRG 4A0A8.c:2256), typed Event::ScriptSetBackgroundOffset."""

from event_room_commands_common import BACKGROUND_OFFSET, configure

globals().update(
    configure(
        1139,
        BACKGROUND_OFFSET,
        0x800BA218,
        0x800BA264,
        {
            "second-half-at-2": {0x800BA230: 0x26040002},
            "first-half-unsigned": {0x800BA23C: 0x00108402},
            "second-half-unsigned": {0x800BA24C: 0x00022C02},
        },
    )
)
