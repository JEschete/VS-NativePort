"""F00657 func_80090B28 (BATTLE.PRG 146C.c:18221), typed Effects::WeatherVelocityOf."""

from weather_pool_common import VELOCITY, configure

globals().update(
    configure(
        657,
        VELOCITY,
        0x80090B28,
        0x80090C2C,
        {
            "elevation-for-azimuth": {0x80090B44: 0x86240008},
            "x-floored": {0x80090B74: 0x00000000},
            "z-not-negated": {0x80090BB0: 0x00000000},
            "y-scale-from-azimuth": {0x80090BF0: 0x8623000A},
        },
    )
)
