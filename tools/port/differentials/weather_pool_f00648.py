"""F00648 func_8008F29C (BATTLE.PRG 146C.c:17723), typed Effects::InitializeFallingWeatherParticles."""

from weather_pool_common import FALLING, configure

globals().update(
    configure(
        648,
        FALLING,
        0x8008F29C,
        0x8008F30C,
        {
            "kind-dropped": {0x8008F2C0: 0x00000000},
            "capacity-dropped": {0x8008F2C8: 0x00000000},
            "half-stride": {0x8008F2F0: 0x26310008},
        },
    )
)
