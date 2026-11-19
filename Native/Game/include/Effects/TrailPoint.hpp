#pragma once

#include <cstdint>

namespace VS::Effects {
// A camera-relative trail point: an SVECTOR without its pad. Bone trails and
// spline trails share it.
struct TrailPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const TrailPoint&, const TrailPoint&) = default;
};
}  // namespace VS::Effects
