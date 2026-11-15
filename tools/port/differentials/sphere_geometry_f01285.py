"""F01285 arc sampling and flag emission."""

from differentials.sphere_geometry_common import configure

configure(
    globals(),
    1285,
    0,
    0x800C0758,
    0x800C085C,
    ("constexpr std::int32_t kArcPhaseMask = 31;",
     "constexpr std::int32_t kArcPhaseMask = 30;"),
)
