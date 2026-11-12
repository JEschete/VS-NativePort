#pragma once

#include <cstdint>
#include <span>

#include "Battle/StatFactors.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Battle {
using HitLimbAmounts = std::span<std::int16_t, Items::kLimbCount>;
inline constexpr std::size_t kArmorLossSlots = 8;
using ArmorLossAmounts = std::span<std::int16_t, kArmorLossSlots>;
using DamageGem = Items::EquipmentPart;
using DamageGems = std::span<const DamageGem, Items::kWeaponGemCount>;

std::int16_t ApplyDamageGemMultiplier(std::uint8_t action_type,
                                      std::int16_t damage, DamageGems weapon,
                                      DamageGems shield);
enum class LimbDistribution { kEven, kSplash, kSelected, kNone };
LimbDistribution DamageDistribution(std::uint8_t category);
void ClearHitLimbArmorLoss(ArmorLossAmounts armor_loss);
// Only even distribution uses vitals; only splash uses its selected row.
// Missing/invalid used inputs refuse after clearing the first six DP entries.
bool DistributeHitLimbDamage(std::uint8_t category, int selected_limb,
                             std::int16_t amount,
                             std::span<const StatFactorVital> vitals,
                             std::span<const std::uint8_t> splash,
                             HitLimbAmounts hit, ArmorLossAmounts armor_loss);
void DistributeLimbRecovery(
    std::int16_t amount,
    std::span<const StatFactorVital, Items::kLimbCount> vitals,
    HitLimbAmounts hit, ArmorLossAmounts armor_loss);
std::int16_t LastActionStatValue(std::uint32_t last_value);
std::int16_t ScaleLastActionStatValue(std::uint32_t last_value,
                                      std::uint8_t factor);
std::int16_t TargetRisk(std::uint16_t risk);
}  // namespace VS::Battle
