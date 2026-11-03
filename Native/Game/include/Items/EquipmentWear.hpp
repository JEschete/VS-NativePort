#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Items/ItemRecords.hpp"

// Equipment wear: the DP a hit takes off the attacker's weapon and the
// defender's raised shield and struck armour, and the PP the hit adds.
namespace VS::Items {
// Gem effect that raises the PP a weapon gains (and lowers a shield's).
inline constexpr std::uint8_t kPpGemEffect = 9;

// `base_pp_amount` scaled by 105% when one of the weapon's gems has the PP
// effect (100% otherwise), truncated toward zero.
[[nodiscard]] std::int16_t ApplyPpGemMultiplier(std::int16_t base_pp_amount,
                                                const WeaponLoadout& weapon);

// Take `amount` DP off the attacker's weapon (unless the action is a spell)
// and off the defender's shield when it is raised, and each limb's loss in
// `limb_dp_loss` off that limb's armour; DP stops at 0. An unknown
// weapon-drawn state changes nothing and returns kActorOutOfRange.
[[nodiscard]] ItemsStatus ConsumeHitEquipmentDp(
    const ItemAction& action, WeaponLoadout& attacker_weapon,
    DefenderEquipment& defender,
    std::span<const std::int16_t, kLimbCount> limb_dp_loss,
    std::int32_t amount);

// Add a tenth of `dp_amount` as PP to the attacker's weapon (unless the action
// is a spell; +5% with a PP gem) and to the defender's raised shield (-5%
// with a PP gem); PP stops at its maximum. An unknown weapon-drawn state
// changes nothing and returns kActorOutOfRange.
[[nodiscard]] ItemsStatus AccumulateHitEquipmentPp(
    const ItemAction& action, WeaponLoadout& attacker_weapon,
    DefenderEquipment& defender, std::int32_t dp_amount);
}  // namespace VS::Items
