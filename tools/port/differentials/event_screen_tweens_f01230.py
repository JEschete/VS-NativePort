"""F01230 func_800BDAB4 (BATTLE/BATTLE.PRG 4A0A8.c:3812), typed Event::UpdateEventScreenTransforms."""

from event_screen_tweens_common import TWEEN_SET, configure

globals().update(
    configure(
        1230,
        TWEEN_SET,
        0x800BDAB4,
        0x800BDBB4,
        {
            "tint-green-high": {0x800BDB24: 0x90620221},
            "warp-scale-growth": {0x800BDB94: 0x84440260},
            "scale-always": {0x800BDAD0: 0x00000000},
        },
    )
)
