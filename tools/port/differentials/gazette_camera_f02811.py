"""F02811 _renderGradientQuad (MENU9.PRG 238.c:1594), typed Menu::Score::DrawGradientPanel."""

from gazette_camera_common import PANEL, configure

globals().update(
    configure(
        2811,
        PANEL,
        0x801051AC,
        0x8010539C,
        {
            "quad-length-9": {0x801051D8: 0x24020009},
            "quad-semi-transparent": {0x80105208: 0x2402003A},
            "right-edge-one-past": {0x801051E0: 0x00000000},
            "bottom-edge-one-past": {0x80105200: 0x00000000},
            "right-colour-from-left-stops": {0x8010521C: 0x24422820},
            "shadow-x-plus-3": {0x801052A0: 0x26520003},
            "quad-in-fade-slot": {0x8010529C: 0x24840020},
            "additive-page": {0x8010530C: 0x24190040},
        },
    )
)