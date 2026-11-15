"""F01117 func_800B983C (BATTLE.PRG 4A0A8.c:2000), typed Event::ScriptHideGeometryGroup."""

from event_room_commands_common import HIDE, configure

globals().update(
    configure(
        1117,
        HIDE,
        0x800B983C,
        0x800B9860,
        {
            "group-signed": {0x800B9844: 0x80840001},
            "group-reads-second": {0x800B9844: 0x90840002},
            "returns-hold": {0x800B9854: 0x24020001},
        },
    )
)
