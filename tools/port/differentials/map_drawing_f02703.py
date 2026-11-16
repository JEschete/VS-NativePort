"""F02703 _drawUIControlsBackground (MENU5.PRG 4D8.c:1965), typed Menu::map::DrawControlBackdrop."""

from map_drawing_common import BACKDROP, configure

globals().update(
    configure(
        2703,
        BACKDROP,
        0x80106AF4,
        0x80106C84,
        {
            "rows-one-more": {0x80106B28: 0x24430000},
            "line-code-0x52": {0x80106B64: 0x24020052},
            "width-not-shrunk": {0x80106BC4: 0x00000000},
            "shadow-offset-1": {0x80106BDC: 0x26730001},
            "shadow-slot-0x18": {0x80106C48: 0x24840018},
            "cursor-short": {0x80106C4C: 0x26220014},
        },
    )
)
