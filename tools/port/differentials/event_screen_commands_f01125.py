"""F01125 func_800B9B18 (BATTLE.PRG 4A0A8.c:2079), typed Event::ScriptSetRoomDisplayFlags."""

from event_screen_commands_common import DISPLAY_FLAGS, configure

globals().update(
    configure(
        1125,
        DISPLAY_FLAGS,
        0x800B9B18,
        0x800B9BF8,
        {
            "display-one-bit": {0x800B9B30: 0x30430001},
            "room-shift-3": {0x800B9B6C: 0x000210C2},
            "actors-shift-5": {0x800B9BAC: 0x00021142},
        },
    )
)
