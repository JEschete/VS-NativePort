"""F00636 func_8008E8F8 (BATTLE.PRG 146C.c:17445), typed Effects::AllocateWeatherParticles."""

from weather_pool_common import ALLOCATE, configure

globals().update(
    configure(
        636,
        ALLOCATE,
        0x8008E8F8,
        0x8008E938,
        {
            "free-skipped": {0x8008E914: 0x00000000},
            "free-always": {0x8008E90C: 0x00000000},
            "size-halved": {0x8008E920: 0x24040400},
        },
    )
)
