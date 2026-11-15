"""F00649 func_8008F30C (BATTLE.PRG 146C.c:17737), typed Effects::InitializeScreenWeatherParticles."""

from weather_pool_common import SCREEN, configure

globals().update(
    configure(
        649,
        SCREEN,
        0x8008F30C,
        0x8008F440,
        {
            "fade-kept": {0x8008F330: 0x00000000},
            "unprojected-zero": {0x8008F35C: 0x24130000},
            "screen-x-raw": {0x8008F3A0: 0x00000000},
            "depth-floored": {0x8008F3E8: 0x10000002},
        },
    )
)
