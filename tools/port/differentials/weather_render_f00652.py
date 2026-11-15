"""F00652 func_8008FA14 (BATTLE.PRG 146C.c:17878), typed Effects::ResetRisingEmberParticle."""

from weather_render_common import EMBER_RESET, configure

globals().update(
    configure(
        652,
        EMBER_RESET,
        0x8008FA14,
        0x8008FAC8,
        {
            "quotient-for-remainder": {0x8008FA40: 0x00001812},
            "depth-halved": {0x8008FA98: 0x24420100},
            "speed-floor-nine": {0x8008FAAC: 0x24420009},
        },
    )
)
