"""F02659 MENU4.PRG _renderUnknownValue (120.c:692), typed Menu::Status::DrawUnanalyzedValue."""

from status_switching_common import UNKNOWN, configure

globals().update(
    configure(
        2659,
        UNKNOWN,
        0x8010399C,
        0x80103A6C,
        {
            "small-glyph-wide": {0x801039F4: 0x2610FFFC},
            "separator-skipped": {0x80103A04: 0x00000000},
            "large-uv-small": {0x80103A20: 0x365200E4},
            "row-kept": {0x80103A14: 0x00000000},
            "two-large": {0x80103A44: 0x2A220002},
        },
    )
)
