"""F01229 func_800BD6C4 (BATTLE/BATTLE.PRG 4A0A8.c:3691), typed Event::CommandScreenTransform."""

from event_screen_tweens_common import TRANSFORM, configure

globals().update(
    configure(
        1229,
        TRANSFORM,
        0x800BD6C4,
        0x800BDAB4,
        {
            "busy-2": {0x800BD930: 0x24020002},
            "offset-unsigned": {0x800BD8CC: 0x00021602},
            "scale-target-shift-6": {0x800BD798: 0x00021180},
        },
    )
)
