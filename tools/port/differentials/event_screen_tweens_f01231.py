"""F01231 func_800BDBB4 (BATTLE/BATTLE.PRG 4A0A8.c:3838), typed Event::UpdateEventTripleTween."""

from event_screen_tweens_common import TWEEN, configure

globals().update(
    configure(
        1231,
        TWEEN,
        0x800BDBB4,
        0x800BDC9C,
        {
            "stops-unequal": {0x800BDC7C: 0x10A30002},
            "scratch-0x8c": {0x800BDC08: 0x3484008C},
            "tick-quarter": {0x800BDBEC: 0x00021083},
        },
    )
)
