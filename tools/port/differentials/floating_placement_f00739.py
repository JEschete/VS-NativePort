"""F00739 equipment stat-change row (func_8009695C) against its complete original instructions and AddPrim."""

from differentials.floating_placement_common import EQUIP_DRAW, configure

globals().update(
    configure(
        739,
        EQUIP_DRAW,
        0x8009695C,
        0x80096FF0,
        {
            "colour-index-halved": {0x800969DC: 0x00111040},
            "first-word-lowered": {0x80096A08: 0x00A44021},
            "first-word-table-shifted": {0x80096A1C: 0x2610914C},
            "word-page": {0x80096B7C: 0x2413000D},
            "sign-atlas-u": {0x80096E04: 0x26230079},
            "sign-atlas-v": {0x80096E44: 0x240200D1},
            "word-gap-three": {0x80096E70: 0x27DE0003},
            "digit-bottom-ten": {0x80096ED4: 0x2502000A},
            "digit-palette": {0x80096F68: 0x240237F5},
            "stat-change-layer": {0x80096FA4: 0x2484000C},
            "digit-advance-four": {0x80096FB4: 0x27DEFFFC},
        },
    )
)
