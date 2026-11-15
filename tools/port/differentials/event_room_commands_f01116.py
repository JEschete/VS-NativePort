"""F01116 func_800B9818 (BATTLE.PRG 4A0A8.c:1994), typed Event::ScriptShowGeometryGroup."""

from event_room_commands_common import SHOW, configure

globals().update(
    configure(
        1116,
        SHOW,
        0x800B9818,
        0x800B983C,
        {
            "group-signed": {0x800B9820: 0x80840001},
            "group-reads-second": {0x800B9820: 0x90840002},
            "returns-hold": {0x800B9830: 0x24020001},
        },
    )
)
