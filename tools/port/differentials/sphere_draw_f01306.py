"""F01306 sphere camera basis (func_800C282C): yaw rotation entries and recorded yaw."""

from differentials.sphere_draw_common import configure

configure(
    globals(),
    1306,
    {
        "cosine-of-zero": {0x800C2858: 0x00002021},
        "first-entry-sine": {0x800C2870: 0xA6040000},
        "sine-not-negated": {0x800C2880: 0x00000000},
        "yaw-from-cosine": {0x800C288C: 0xA4720022},
    },
    ("0U - static_cast<std::uint32_t>(sine)", "static_cast<std::uint32_t>(sine)"),
)
