"""F01286 volume rotation and scale transform."""

from differentials.sphere_geometry_common import configure

configure(
    globals(),
    1286,
    1,
    0x800C085C,
    0x800C0990,
    ("constexpr std::int32_t kVolumeScale = 32;",
     "constexpr std::int32_t kVolumeScale = 31;"),
)
