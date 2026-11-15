"""F01304 cone ribs and closing circles."""

from differentials.sphere_geometry_common import configure

configure(
    globals(),
    1304,
    3,
    0x800C2368,
    0x800C253C,
    ("constexpr std::int32_t kConeStepScale = kFixedOne / 8;",
     "constexpr std::int32_t kConeStepScale = kFixedOne / 7;"),
)
