"""F01140 func_800BA264 (BATTLE.PRG 4A0A8.c:2262), typed Event::ScriptSetFirstPersonView."""

from event_room_commands_common import FIRST_PERSON, configure

globals().update(
    configure(
        1140,
        FIRST_PERSON,
        0x800BA264,
        0x800BA288,
        {
            "operand-unnormalized": {0x800BA274: 0x00000000},
            "operand-inverted": {0x800BA274: 0x2C840001},
            "returns-hold": {0x800BA27C: 0x24020001},
        },
    )
)
