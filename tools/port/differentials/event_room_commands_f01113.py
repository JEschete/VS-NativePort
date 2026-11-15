"""F01113 func_800B979C (BATTLE.PRG 4A0A8.c:1972), typed Event::ScriptOverrideDepthCueDistance."""

from event_room_commands_common import DEPTH_CUE, configure

globals().update(
    configure(
        1113,
        DEPTH_CUE,
        0x800B979C,
        0x800B97E4,
        {
            "depth-mask-10-bits": {0x800B97D0: 0x308403FF},
            "sign-from-bit-14": {0x800B97B4: 0x00041440},
            "returns-hold": {0x800B97D8: 0x24020001},
        },
    )
)
