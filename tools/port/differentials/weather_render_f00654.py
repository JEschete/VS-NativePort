"""F00654 func_8008FDC4 (BATTLE.PRG 146C.c:17938), typed Effects::DrawSnowParticles."""

from weather_render_common import SNOW, configure

globals().update(
    configure(
        654,
        SNOW,
        0x8008FDC4,
        0x80090434,
        {
            "pair-angle-halved": {0x800900C0: 0x00178100},
            "settled-shade-112": {0x800902EC: 0x2403FF90},
            "sway-period-31": {0x8008FF34: 0x2C630020},
            "texture-page-0x72": {0x800901A8: 0x24180072},
        },
    )
)
