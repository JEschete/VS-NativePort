"""F02813 _renderTopMenu (MENU9.PRG 238.c:1679), typed Menu::Score::RenderOverview."""

from gazette_camera_common import OVERVIEW, configure

globals().update(
    configure(
        2813,
        OVERVIEW,
        0x8010552C,
        0x801056B8,
        {
            "edge-fade-through-step-8": {0x80105538: 0x2AA20009},
            "offset-slide-16": {0x80105574: 0x00031900},
            "offset-y-224": {0x80105584: 0x240200E0},
            "offset-not-restored": {0x801055A4: 0x00000000},
            "caption-x-minus-112": {0x801055D4: 0x2611FF90},
            "top-panel-gradient-3": {0x80105630: 0x24020003},
            "footer-rises-3": {0x80105684: 0x00000000},
            "score-caption-edge-slot": {0x801055F8: 0x24E70020},
            "rank-caption-y-176": {0x80105644: 0x3C0500B0},
        },
    )
)