"""F01132 func_800B9F4C (BATTLE.PRG 4A0A8.c:2166), typed Event::ScriptLoadRoomBackground."""

from event_room_commands_common import BACKGROUND_LOAD, configure

globals().update(
    configure(
        1132,
        BACKGROUND_LOAD,
        0x800B9F4C,
        0x800B9F70,
        {
            "operand-byte-2": {0x800B9F54: 0x90840002},
            "operand-signed": {0x800B9F54: 0x80840001},
            "returns-hold": {0x800B9F64: 0x24020001},
        },
    )
)
