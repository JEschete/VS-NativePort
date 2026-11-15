"""F00650 func_8008F440 (BATTLE.PRG 146C.c:17758), typed Effects::DrawRainParticles."""

from weather_render_common import RAIN, configure

globals().update(
    configure(
        650,
        RAIN,
        0x8008F440,
        0x8008F9A4,
        {
            "odd-check-dropped": {0x8008F6F4: 0x32820000},
            "splash-depth-bias": {0x8008F858: 0x2452FFE0},
            "streak-tail-blue": {0x8008F63C: 0x24020065},
            "undrawn-shift-one": {0x8008F558: 0x24020001},
        },
    )
)
