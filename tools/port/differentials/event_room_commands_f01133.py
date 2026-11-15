"""F01133 func_800B9F70 (BATTLE.PRG 4A0A8.c:2172), typed Event::ScriptWaitForRoomBackground."""

from event_room_commands_common import BACKGROUND_WAIT, configure

globals().update(
    configure(
        1133,
        BACKGROUND_WAIT,
        0x800B9F70,
        0x800B9F90,
        {
            "ready-inverted": {0x800B9F84: 0x0002102B},
            "returns-callee-result": {0x800B9F84: 0x00000000},
            "always-advances": {0x800B9F84: 0x00001021},
        },
    )
)
