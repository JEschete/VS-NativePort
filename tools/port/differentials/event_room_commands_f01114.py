"""F01114 func_800B97E4 (BATTLE.PRG 4A0A8.c:1982), typed Event::ScriptSetRoomLightingMode."""

from event_room_commands_common import LIGHTING, configure

globals().update(
    configure(
        1114,
        LIGHTING,
        0x800B97E4,
        0x800B97F8,
        {
            "mode-signed": {0x800B97E4: 0x80830001},
            "mode-reads-second": {0x800B97E4: 0x90830002},
            "returns-hold": {0x800B97F4: 0x24020001},
        },
    )
)
