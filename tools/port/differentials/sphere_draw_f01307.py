"""F01307 sphere position cursor (func_800C28AC): projected point, blink, pitch quads and sprites."""

from differentials.sphere_draw_common import configure

configure(
    globals(),
    1307,
    {
        "blink-mask-7": {0x800C2914: 0x30420007},
        "quad-color-0x2c81": {0x800C2928: 0x3C0D2C81},
        "first-texcoord-color": {0x800C29B0: 0xAF0D000C},
        "cursor-not-advanced": {0x800C2A48: 0x00000000},
        "highlight-pose-3": {0x800C2A78: 0x24020003},
        "plain-brightness-0x81": {0x800C2A80: 0x24040081},
        "texcoord-word-5": {0x800C2AD0: 0xAC430014},
    },
    ("constexpr std::uint8_t kBlinkMask = 0xF;", "constexpr std::uint8_t kBlinkMask = 0x7;"),
)
