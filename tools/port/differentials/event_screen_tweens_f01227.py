"""F01227 func_800BD57C (BATTLE/BATTLE.PRG 4A0A8.c:3647), typed Event::UpdateEventAngleTween."""

from event_screen_tweens_common import ANGLE, configure

globals().update(
    configure(
        1227,
        ANGLE,
        0x800BD57C,
        0x800BD610,
        {
            "stops-early": {0x800BD5EC: 0x10600002},
            "tick-quarter": {0x800BD5AC: 0x00021083},
            "fraction-11": {0x800BD5E4: 0x00072AC3},
        },
    )
)
