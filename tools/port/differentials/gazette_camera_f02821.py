"""F02821 _renderScreenEdgeFade (MENU9.PRG 238.c:2146), typed Menu::Score::DrawViewerEdgeFade."""

from gazette_camera_common import FADE, configure

globals().update(
    configure(
        2821,
        FADE,
        0x80106528,
        0x801066F0,
        {
            "left-fade-width-32": {0x80106544: 0x24020020},
            "left-fade-opaque": {0x80106578: 0x00000000},
            "fade-to-224": {0x8010654C: 0x241200E0},
            "right-fade-from-272": {0x801065E0: 0x24030110},
            "left-fade-panel-slot": {0x801065D8: 0x2484001C},
            "page-panel-slot": {0x8010665C: 0x24060007},
            "opaque-page": {0x80106660: 0x24070060},
            "fade-colour-64": {0x80106560: 0x24110040},
        },
        vectors=32,
    )
)