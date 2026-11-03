// Hand-maintained (tools/port/owned.csv).
#include "Items/Recovery.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Items/ItemRecords.hpp"

namespace VS::Items {
namespace {
constexpr std::uint32_t kRecoveryPerStatFactor = 5;
// The averaged draw in [0, 11) is centred on 0.
constexpr std::int32_t kVariationDrawBound = 11;
constexpr std::uint32_t kVariationCentre = 5;
}  // namespace

// decomp: BATTLE.PRG _itemRecovery (src/BATTLE/BATTLE.PRG/146C.c:12109)
// The original indexed the hits with any index and took two hit results it
// never read.
std::optional<std::int16_t> CalculateItemRecovery(const ItemAction& action,
                                                  std::int32_t hit_index,
                                                  bool add_random_variation,
                                                  ItemRandom& random) {
  if (hit_index < 0 ||
      static_cast<std::size_t>(hit_index) >= action.hit_stat_factors.size()) {
    return std::nullopt;
  }
  const std::uint32_t base =
      action.hit_stat_factors[static_cast<std::size_t>(hit_index)] *
      kRecoveryPerStatFactor;
  const std::uint32_t variation =
      add_random_variation ? static_cast<std::uint32_t>(
                                 random.NextAveraged(kVariationDrawBound)) -
                                 kVariationCentre
                           : 0U;
  // The R3000 keeps the wrapped sum to 16 bits before the sign test.
  const auto recovery = static_cast<std::int16_t>(base + variation);
  return recovery < 0 ? std::int16_t{0} : recovery;
}
}  // namespace VS::Items
