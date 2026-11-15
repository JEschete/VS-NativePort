"""F01119 func_800B988C (BATTLE.PRG 4A0A8.c:2012), typed Event::ScriptSetCollisionTileMode."""

from event_room_commands_common import COLLISION, configure

globals().update(
    configure(
        1119,
        COLLISION,
        0x800B988C,
        0x800B98C0,
        {
            "z-reads-layer": {0x800B989C: 0x90450002},
            "layer-reads-z": {0x800B98A0: 0x90460003},
            "blocked-signed": {0x800B98A4: 0x80470004},
            "returns-hold": {0x800B98B4: 0x24020001},
        },
    )
)
