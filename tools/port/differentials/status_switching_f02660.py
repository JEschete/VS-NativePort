"""F02660 MENU4.PRG _renderStatBar (120.c:721), typed Menu::Status::DrawVitalBar."""

from status_switching_common import BAR, configure

globals().update(
    configure(
        2660,
        BAR,
        0x80103A6C,
        0x80103AC8,
        {
            "zero-max-kept": {0x80103A70: 0x10000003},
            "always-full": {0x80103A84: 0x00000000},
            "no-round-up": {0x80103A98: 0x00000000},
            "color-unmasked": {0x80103AAC: 0x00000000},
            "chain-offset": {0x80103AB4: 0x24C6FFF0},
        },
    )
)
