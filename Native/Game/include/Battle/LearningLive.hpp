#pragma once

#include <optional>

namespace VS::Battle {
// Missing catalog/unsupported compatibility references refuse, never zero.
[[nodiscard]] std::optional<int> LiveBreakArtRequirement(int category,
                                                         int learned);
[[nodiscard]] std::optional<int> LiveBattleAbilityRequirement(int unlocked);
}  // namespace VS::Battle
