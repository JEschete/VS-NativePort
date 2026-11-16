"""F00720 value-label placement search (func_80094844) against its complete original instructions."""

from differentials.floating_placement_common import PLACE, configure

globals().update(
    configure(
        720,
        PLACE,
        0x80094844,
        0x80094AF8,
        {
            "unit-scale-off-by-one": {0x80094848: 0x24021001},
            "overlap-width-sixteen": {0x800948AC: 0x28420010},
            "column-step-nine": {0x80094A1C: 0x24E70009},
            "left-offset-not-negated": {0x80094A18: 0x00000000},
            "bottom-clamp-two-hundred-one": {0x80094ADC: 0x240200C9},
            "count-advances-two": {0x80094AEC: 0x24420002},
        },
    )
)
