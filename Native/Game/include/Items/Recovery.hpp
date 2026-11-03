#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Items/ItemRecords.hpp"

// Healing items: the HP or MP an item's hit restores.
namespace VS::Items {
// Five points per point of hit `hit_index`'s stat factor, plus (when
// `add_random_variation`) an averaged draw in [-5, 5], never below 0. A hit
// index outside the action's two hits returns nothing.
[[nodiscard]] std::optional<std::int16_t> CalculateItemRecovery(
    const ItemAction& action, std::int32_t hit_index, bool add_random_variation,
    ItemRandom& random);
}  // namespace VS::Items
