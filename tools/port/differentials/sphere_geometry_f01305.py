"""F01305 command dispatch and wireframe state advancement."""

from differentials.sphere_geometry_common import configure

configure(
    globals(),
    1305,
    4,
    0x800C253C,
    0x800C282C,
    ("shape += type * 16;", "shape += type * 15;"),
)
