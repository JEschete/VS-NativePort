"""F01289 sphere traversal (func_800C0D78): sine samples, staging, projected depth and per-kind shapes."""

from differentials.sphere_draw_common import configure

configure(
    globals(),
    1289,
    {
        "first-color-0x10402": {0x800C0D98: 0x36940402},
        "sine-step-64": {0x800C0DC8: 0x00112180},
        "depth-shift-3": {0x800C0E54: 0x000C60C3},
        "stretch-lift-whole": {0x800C0EC8: 0x00021403},
        "tilt-quarter-turn": {0x800C0EF4: 0x24420040},
        "kind-7-shape-3": {0x800C0F40: 0x24040003},
        "second-color-0x10105": {0x800C0F5C: 0x36940105},
        "labels-skipped": {0x800C0F78: 0x00000000},
    },
    ("constexpr std::uint8_t kTiltHalfTurn = 0x80;", "constexpr std::uint8_t kTiltHalfTurn = 0x40;"),
)
