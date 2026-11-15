"""F00655 func_80090434 (BATTLE.PRG 146C.c:18072), typed Effects::DrawScreenWeatherParticles."""

from weather_render_common import SCREEN, configure

globals().update(
    configure(
        655,
        SCREEN,
        0x80090434,
        0x80090A1C,
        {
            "far-shade-254": {0x80090758: 0x240300FE},
            "fade-limit-88": {0x8009051C: 0x2C620058},
            "trail-scale-four": {0x800908C8: 0x00021080},
            "depth-scale-eight": {0x80090730: 0x000518C0},
            "texture-page-0x32": {0x8009097C: 0x24180032},
        },
    )
)
