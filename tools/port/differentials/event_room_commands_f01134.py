"""F01134 func_800B9F90 (BATTLE.PRG 4A0A8.c:2174), typed Event::ScriptFreeRoomBackground."""

from event_room_commands_common import BACKGROUND_FREE, configure

globals().update(
    configure(
        1134,
        BACKGROUND_FREE,
        0x800B9F90,
        0x800B9FC0,
        {
            "yields-when-skippable": {0x800B9FAC: 0x38420001},
            "yield-code-2": {0x800B9FB4: 0x00021040},
            "skip-state-ignored": {0x800B9FA4: 0x24020002},
        },
    )
)
