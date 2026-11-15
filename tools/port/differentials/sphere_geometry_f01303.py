"""F01303 closed circle generation."""

from differentials.sphere_geometry_common import configure

configure(
    globals(),
    1303,
    2,
    0x800C2254,
    0x800C2368,
    ("constexpr std::size_t kCircleVertexCount = 33;",
     "constexpr std::size_t kCircleVertexCount = 32;"),
)
