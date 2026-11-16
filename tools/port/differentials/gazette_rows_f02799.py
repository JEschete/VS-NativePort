"""F02799 _renderScrollableMenuItem (MENU9.PRG 238.c:54), typed Menu::Score::RenderScrollableRow."""

from gazette_rows_common import RENDER, configure

globals().update(
    configure(
        2799,
        RENDER,
        0x80102A7C,
        0x80102E10,
        {
            "arrow-x-limit-64": {0x80102B44: 0x2C420040},
            "arrow-indent-13": {0x80102B80: 0x2442FFF3},
            "step-wrap-13": {0x80102B1C: 0x2C42000D},
            "text-indent-7": {0x80102BB4: 0x24530007},
            "line-open-not-dithered": {0x80102C4C: 0x34E70020},
            "shadow-drop-3": {0x80102D10: 0x26860003},
            "fade-kept": {0x80102D40: 0x00000000},
            "reflection-shine-64": {0x80102D78: 0x00028180},
        },
    )
)
