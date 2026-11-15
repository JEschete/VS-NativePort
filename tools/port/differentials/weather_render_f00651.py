"""F00651 func_8008F9A4 (BATTLE.PRG 146C.c:17864), typed Effects::InitializeRisingEmberParticles."""

from weather_render_common import EMBER_INIT, configure

globals().update(
    configure(
        651,
        EMBER_INIT,
        0x8008F9A4,
        0x8008FA14,
        {
            "kind-dropped": {0x8008F9C8: 0x00000000},
            "capacity-dropped": {0x8008F9D0: 0x00000000},
            "half-stride": {0x8008F9F8: 0x26310008},
        },
    )
)
