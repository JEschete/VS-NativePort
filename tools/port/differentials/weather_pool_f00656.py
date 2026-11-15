"""F00656 func_80090A1C (BATTLE.PRG 146C.c:18200), typed Effects::ResetFallingWeatherParticle."""

from weather_pool_common import RESET, configure

globals().update(
    configure(
        656,
        RESET,
        0x80090A1C,
        0x80090B28,
        {
            "origin-x-dropped": {0x80090A5C: 0x00031021},
            "extent-z-from-x": {0x80090A68: 0x8603004C},
            "open-area-floored": {0x80090A90: 0x10000003},
            "height-unbiased": {0x80090AF0: 0x00000000},
            "fall-shift-3": {0x80090AF8: 0x24020003},
        },
    )
)
