"""F01141 func_800BA288 (BATTLE.PRG 4A0A8.c:2268), typed Event::ScriptAdjustGeometryDepthBias."""

from event_room_commands_common import DEPTH_BIAS, configure

globals().update(
    configure(
        1141,
        DEPTH_BIAS,
        0x800BA288,
        0x800BA2B4,
        {
            "bias-unsigned": {0x800BA298: 0x90450002},
            "group-signed": {0x800BA294: 0x80440001},
            "returns-hold": {0x800BA2A8: 0x24020001},
        },
    )
)
