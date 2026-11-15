"""F01131 func_800B9F28 (BATTLE.PRG 4A0A8.c:2160), typed Event::ScriptSetRoomBackgroundVisible."""

from event_room_commands_common import BACKGROUND_VISIBLE, configure

globals().update(
    configure(
        1131,
        BACKGROUND_VISIBLE,
        0x800B9F28,
        0x800B9F4C,
        {
            "operand-byte-2": {0x800B9F30: 0x90840002},
            "operand-signed": {0x800B9F30: 0x80840001},
            "returns-hold": {0x800B9F40: 0x24020001},
        },
    )
)
