"""F01226 func_800BD444 (BATTLE/BATTLE.PRG 4A0A8.c:3601), typed Event::CommandTweenCameraRoll."""

from event_screen_tweens_common import ROLL, configure

globals().update(
    configure(
        1226,
        ROLL,
        0x800BD444,
        0x800BD57C,
        {
            "wrap-half-turn": {0x800BD4FC: 0x2462F800},
            "easing-mask-7": {0x800BD55C: 0x30420007},
            "camera-roll-0x174": {0x800BD484: 0x24500174},
        },
    )
)
