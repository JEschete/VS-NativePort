"""F01138 func_800BA194 (BATTLE.PRG 4A0A8.c:2238), typed Event::ScriptControlRoomAnimation."""

from event_room_commands_common import ROOM_ANIMATION, configure

globals().update(
    configure(
        1138,
        ROOM_ANIMATION,
        0x800BA194,
        0x800BA218,
        {
            "effect-from-mode-byte": {0x800BA1A4: 0x90900001},
            "toggle-mode-2": {0x800BA1AC: 0x24020002},
            "finishes-when-skippable": {0x800BA1C8: 0x24020001},
            "finishes-inactive-effect": {0x800BA1DC: 0x00000000},
        },
    )
)
