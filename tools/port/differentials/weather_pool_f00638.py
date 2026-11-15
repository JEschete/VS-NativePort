"""F00638 func_8008EA90 (BATTLE.PRG 146C.c:17500), typed Effects::InitializeRoomWeather."""

from weather_pool_common import ROOM_WEATHER, configure

globals().update(
    configure(
        638,
        ROOM_WEATHER,
        0x8008EA90,
        0x8008EAE4,
        {
            "rain-never-suppressed": {0x8008EAA4: 0x10000005},
            "kind-unsigned": {0x8008EABC: 0x94A40034},
            "returns-zero": {0x8008EACC: 0x24020000},
        },
    )
)
