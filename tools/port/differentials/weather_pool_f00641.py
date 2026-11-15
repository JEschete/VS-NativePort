"""F00641 func_8008EB04 (BATTLE.PRG 146C.c:17525), typed Effects::RoomWeatherStateOf."""

from weather_pool_common import WEATHER_STATE, configure

globals().update(
    configure(
        641,
        WEATHER_STATE,
        0x8008EB04,
        0x8008EB24,
        {
            "kind-signed": {0x8008EB08: 0x80421DCA},
            "intensity-unsigned": {0x8008EB18: 0x94421DC4},
        },
    )
)
