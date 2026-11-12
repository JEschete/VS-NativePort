// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamagePolicies.hpp"

#include <algorithm>
#include <array>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
constexpr int kPercent = 100;
constexpr int kGemAdjustment = 5;
bool HasGem(DamageGems gems, std::uint8_t effect) {
  return std::any_of(gems.begin(), gems.end(),
                     [effect](const Items::EquipmentPart& gem) {
                       return gem.gem_effects == effect;
                     });
}
std::int16_t Narrow(std::uint32_t value) {
  return Core::Math::WrapSigned16(value);
}
void AddShare(HitLimbAmounts hit, ArmorLossAmounts armor_loss, std::size_t limb,
              std::int32_t share, int hp_multiplier = 1) {
  armor_loss[limb] = Narrow(share);
  hit[limb] = Narrow(hit[limb] + armor_loss[limb] * hp_multiplier);
}
}  // namespace

// decomp: func_8007FE5C (src/BATTLE/BATTLE.PRG/146C.c:10534).
// Original [0x8007FE5C,0x8007FF8C): presence, not a stacking per-gem bonus.
std::int16_t ApplyDamageGemMultiplier(std::uint8_t action_type,
                                      std::int16_t damage, DamageGems weapon,
                                      DamageGems shield) {
  constexpr std::uint8_t kPhysicalSource = 3, kSpellSource = 4;
  constexpr std::uint8_t kPhysicalTarget = 7, kSpellTarget = 8;
  const bool spell = action_type == 1;
  const int percent =
      kPercent +
      (HasGem(weapon, spell ? kSpellSource : kPhysicalSource) ? kGemAdjustment
                                                              : 0) -
      (HasGem(shield, spell ? kSpellTarget : kPhysicalTarget) ? kGemAdjustment
                                                              : 0);
  return Narrow(damage * percent / kPercent);
}

LimbDistribution DamageDistribution(std::uint8_t category) {
  using enum LimbDistribution;
  constexpr std::array kDistribution{kEven,     kEven,   kEven,     kEven,
                                     kEven,     kSplash, kSelected, kSplash,
                                     kSelected, kNone,   kNone,     kSelected,
                                     kSelected, kNone,   kSplash,   kNone};
  return category < kDistribution.size() ? kDistribution[category] : kNone;
}
void ClearHitLimbArmorLoss(ArmorLossAmounts armor_loss) {
  std::fill_n(armor_loss.begin(), Items::kLimbCount, 0);
}

// decomp: func_80080000 (src/BATTLE/BATTLE.PRG/146C.c:10612).
// Original [0x80080000,0x800801E0), SLUS jump table at 0x80068A90.
bool DistributeHitLimbDamage(std::uint8_t category, int selected_limb,
                             std::int16_t amount,
                             std::span<const StatFactorVital> vitals,
                             std::span<const std::uint8_t> splash,
                             HitLimbAmounts hit, ArmorLossAmounts armor_loss) {
  ClearHitLimbArmorLoss(armor_loss);
  const auto kind = DamageDistribution(category);
  if (kind == LimbDistribution::kNone) return true;
  if (kind == LimbDistribution::kEven) {
    if (vitals.size() != Items::kLimbCount) return false;
    const auto count =
        std::count_if(vitals.begin(), vitals.end(),
                      [](StatFactorVital vital) { return vital.maximum != 0; });
    if (count == 0) return true;
    for (std::size_t i = 0; i < hit.size(); ++i) {
      if (vitals[i].maximum != 0) {
        AddShare(hit, armor_loss, i, amount / static_cast<int>(count));
      }
    }
    return true;
  }
  if (selected_limb < 0 ||
      static_cast<std::size_t>(selected_limb) >= hit.size()) {
    return false;
  }
  if (kind == LimbDistribution::kSelected) {
    AddShare(hit, armor_loss, selected_limb, amount);
    return true;
  }
  if (splash.size() != Items::kLimbCount) return false;
  for (std::size_t i = 0; i < hit.size(); ++i) {
    AddShare(hit, armor_loss, i, amount * splash[i] / kPercent);
  }
  return true;
}

// decomp: func_800801E0 (src/BATTLE/BATTLE.PRG/146C.c:10664).
// Original [0x800801E0,0x800802C4): no DP reset, including total cancellation.
void DistributeLimbRecovery(
    std::int16_t amount,
    std::span<const StatFactorVital, Items::kLimbCount> vitals,
    HitLimbAmounts hit, ArmorLossAmounts armor_loss) {
  int total = 0;
  for (const auto vital : vitals) {
    if (vital.maximum != 0) total += vital.maximum - vital.current;
  }
  if (total == 0) return;
  for (std::size_t i = 0; i < hit.size(); ++i) {
    if (vitals[i].maximum != 0) {
      const auto share =
          amount * (vitals[i].maximum - vitals[i].current) / total;
      // SLL 16/SRA 15 doubles the sign-extended low half, not the full DIV.
      AddShare(hit, armor_loss, i, share, 2);
    }
  }
}

// decomp: _getLastValue (src/BATTLE/BATTLE.PRG/146C.c:11237).
std::int16_t LastActionStatValue(std::uint32_t last_value) {
  return Narrow(last_value);
}
// decomp: _factorLastValue (src/BATTLE/BATTLE.PRG/146C.c:12228).
std::int16_t ScaleLastActionStatValue(std::uint32_t last_value,
                                      std::uint8_t factor) {
  constexpr std::uint32_t kTenths = 10;
  const std::uint32_t product = last_value * factor;
  return Narrow(product / kTenths);
}
// decomp: _getRisk (src/BATTLE/BATTLE.PRG/146C.c:12241).
std::int16_t TargetRisk(std::uint16_t risk) { return Narrow(risk); }
}  // namespace VS::Battle
