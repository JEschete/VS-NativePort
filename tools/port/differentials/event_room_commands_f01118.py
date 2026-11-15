"""F01118 func_800B9860 (BATTLE.PRG 4A0A8.c:2006), typed Event::ScriptSwapGeometryGroupFlags."""

from event_room_commands_common import SWAP, configure

globals().update(
    configure(
        1118,
        SWAP,
        0x800B9860,
        0x800B988C,
        {
            "first-reads-second": {0x800B986C: 0x90440002},
            "second-signed": {0x800B9870: 0x80450002},
            "returns-hold": {0x800B9880: 0x24020001},
        },
    )
)
