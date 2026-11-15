"""F00653 func_8008FAC8 (BATTLE.PRG 146C.c:17889), typed Effects::DrawRisingEmberParticles."""

from weather_render_common import EMBER_DRAW, configure

globals().update(
    configure(
        653,
        EMBER_DRAW,
        0x8008FAC8,
        0x8008FDC4,
        {
            "pause-ignored": {0x8008FB50: 0x00000000},
            "near-depth-five": {0x8008FBDC: 0x2A420005},
            "short-flame": {0x8008FC00: 0x24420080},
            "texture-page-0x40": {0x8008FD08: 0x24180040},
        },
    )
)
