"""F01287 sphere wire line (func_800C0990): depth cull, octant nudge, intensity scale and OT link."""

from differentials.sphere_draw_common import configure

configure(
    globals(),
    1287,
    {
        "depth-limit-0x400": {0x800C09B0: 0x2DA20400},
        "nudge-x-bit0": {0x800C0A74: 0x24024FC4},
        "octant-sign-kept": {0x800C0A5C: 0x00000000},
        "intensity-clamp-62": {0x800C0AB4: 0x240B003E},
        "mode-not-stored": {0x800C0AF4: 0x00000000},
        "end-from-start": {0x800C0B28: 0x8D020000},
        "link-not-masked": {0x800C0B38: 0x00000000},
    },
    ("constexpr std::int32_t kMaxIntensity = 63;", "constexpr std::int32_t kMaxIntensity = 62;"),
)
