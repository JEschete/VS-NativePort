#include "Menu/MagicSpellMenu.hpp"

#include <algorithm>

namespace VS::Menu::Magic {
namespace {

constexpr int kGlobalSpellRestrictionMask = 0xB7;
constexpr int kUnselectableRowType = 1;

}  // namespace

std::optional<AvailableSpellList> BuildAvailableSpellList(
    std::span<const SpellCandidate> candidates) noexcept {
  if (candidates.size() > AvailableSpellList::kCapacity) {
    return std::nullopt;
  }

  AvailableSpellList result{};
  for (const SpellCandidate& candidate : candidates) {
    if (!candidate.unlocked) {
      continue;
    }

    result.spells[result.count++] = {
        .source_index = candidate.source_index,
        .action_id = candidate.action_id,
    };
  }
  return result;
}

void ApplyGlobalSpellRestrictions(const AvailableSpellList& available,
                                  std::span<int> row_types,
                                  int action_restrictions) noexcept {
  if ((action_restrictions & kGlobalSpellRestrictionMask) == 0) {
    return;
  }
  const std::size_t populated_count =
      std::min(available.count, row_types.size());
  for (std::size_t row = 0; row < populated_count; ++row) {
    row_types[row] |= kUnselectableRowType;
  }
}

}  // namespace VS::Menu::Magic
