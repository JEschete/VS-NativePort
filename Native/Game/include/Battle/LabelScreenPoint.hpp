#pragma once

#include <cstdint>

namespace VS::Battle {
// A label's screen position as the original packs it (a DVECTOR): x in the low
// halfword and y in the high halfword of a GPU word. The sphere's target labels
// and the floating labels (one VS::Battle::placedLabelPositions entry or a
// projected screen point) share it.
struct LabelScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const LabelScreenPoint&,
                         const LabelScreenPoint&) = default;
};
}  // namespace VS::Battle
