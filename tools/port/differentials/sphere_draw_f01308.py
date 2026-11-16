"""F01308 sphere terrain row cache (func_800C2B0C): bounds, blocked/door cells, gimmick lift and samples."""

from differentials.sphere_draw_common import configure

configure(
    globals(),
    1308,
    {
        "row-limit-8": {0x800C2B14: 0x2CA20008},
        "width-shift-5": {0x800C2B88: 0x00021143},
        "blocked-0xf081": {0x800C2C20: 0x3C17F081},
        "door-limit-4": {0x800C2C38: 0x28420004},
        "low-gimmick-limit-1": {0x800C2CA0: 0x2C420001},
        "low-gimmick-lift-0x10": {0x800C2CB8: 0x24420010},
        "ceiling-shift-18": {0x800C2D24: 0x00022483},
        "door-flag-limit-5": {0x800C2D80: 0x2C420005},
        "door-flag-times-3": {0x800C2DA0: 0x00000000},
        "clearance-0x31": {0x800C2DAC: 0x26C30031},
    },
    ("constexpr std::int32_t kCeilingClearance = 0x30;", "constexpr std::int32_t kCeilingClearance = 0x2F;"),
)
