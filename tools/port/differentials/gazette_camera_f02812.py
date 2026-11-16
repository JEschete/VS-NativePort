"""F02812 _renderFooterBg (MENU9.PRG 238.c:1645), typed Menu::Score::DrawFooterBackground."""

from gazette_camera_common import FOOTER, configure

globals().update(
    configure(
        2812,
        FOOTER,
        0x8010539C,
        0x8010552C,
        {
            "fade-opaque": {0x801053EC: 0x00000000},
            "fade-height-4": {0x801053DC: 0x26310004},
            "width-256": {0x801053C4: 0x24120100},
            "black-to-224": {0x8010545C: 0x240200E0},
            "fade-colour-64": {0x801053E8: 0x24020040},
            "opaque-page": {0x801054A4: 0x24070060},
            "fade-in-edge-slot": {0x80105448: 0x24840020},
        },
        vectors=128,
    )
)