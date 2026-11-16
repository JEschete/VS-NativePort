"""F02810 _renderEnemyInfoBg (MENU9.PRG 238.c:1552), typed Menu::Score::DrawEnemyBanner."""

from gazette_camera_common import BANNER, configure

globals().update(
    configure(
        2810,
        BANNER,
        0x80104F74,
        0x801051AC,
        {
            "line-length-5": {0x80105010: 0x24020005},
            "line-semi-transparent": {0x80105018: 0x24020052},
            "right-edge-not-slanted": {0x80105054: 0x00000000},
            "line-end-one-past": {0x80105030: 0x00000000},
            "shadow-x-plus-1": {0x80105098: 0x26940001},
            "shadow-corner-not-slanted": {0x801050D4: 0x00000000},
            "lines-in-fade-slot": {0x8010507C: 0x24840020},
            "additive-page": {0x80105110: 0x24180040},
            "right-colour-from-left-stops": {0x80104FE8: 0x24422820},
            "one-more-line": {0x80105084: 0x00000000},
        },
    )
)