#pragma once

#include <cstdint>

#include "Items/ItemRecords.hpp"

namespace VS::Items::Live {
// The game's random draws (SLUS_010.40 vs_main_getRand*).
class LiveItemRandom final : public ItemRandom {
 public:
  [[nodiscard]] std::int32_t NextBounded(std::int32_t bound) override;
  [[nodiscard]] std::int32_t NextAveraged(std::int32_t bound) override;
};
}  // namespace VS::Items::Live
