// Hand-maintained (tools/port/owned.csv).
#include "Items/EquipmentWear.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Items/ItemRecords.hpp"

namespace VS::Items {
namespace {
constexpr std::int32_t kWholePercent = 100;
constexpr std::int32_t kPpGemPercent = 5;
// A hit adds a tenth of its DP as PP.
constexpr std::int32_t kDpPerPp = 10;

bool HasPpGem(std::span<const EquipmentPart, kWeaponGemCount> gems) {
  return std::any_of(gems.begin(), gems.end(), [](const EquipmentPart& gem) {
    return gem.gem_effects == kPpGemEffect;
  });
}

// The R3000 wraps the 32-bit products and sums below (mult/mflo, addu, subu).
constexpr std::int32_t WrappingProduct(std::int32_t lhs, std::int32_t rhs) {
  return static_cast<std::int32_t>(static_cast<std::int64_t>(lhs) * rhs);
}

constexpr std::int32_t WrappingSum(std::int32_t lhs, std::int32_t rhs) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(lhs) +
                                   static_cast<std::uint32_t>(rhs));
}

// DP after a signed loss; it stops at 0.
constexpr std::uint16_t LessDp(std::uint16_t dp, std::int32_t loss) {
  return dp < loss
             ? std::uint16_t{0}
             : static_cast<std::uint16_t>(static_cast<std::uint32_t>(dp) -
                                          static_cast<std::uint32_t>(loss));
}

constexpr std::int32_t PpGain(std::int32_t dp_amount, std::int32_t percent) {
  return WrappingProduct(dp_amount / kDpPerPp, percent) / kWholePercent;
}

// PP after a gain; it stops at the maximum.
constexpr std::uint16_t RaisedPp(std::uint16_t pp, std::uint16_t max_pp,
                                 std::int32_t gain) {
  const auto raised = WrappingSum(pp, gain);
  return raised < max_pp ? static_cast<std::uint16_t>(raised) : max_pp;
}
}  // namespace

// decomp: BATTLE.PRG func_8007FF8C (src/BATTLE/BATTLE.PRG/146C.c:10594)
// The original scanned the gems through the actor record (byte 0xCA); it took
// an argument it never read. No code calls it.
std::int16_t ApplyPpGemMultiplier(std::int16_t base_pp_amount,
                                  const WeaponLoadout& weapon) {
  const auto percent =
      kWholePercent + (HasPpGem(weapon.gems) ? kPpGemPercent : 0);
  return static_cast<std::int16_t>(base_pp_amount * percent / kWholePercent);
}

// decomp: BATTLE.PRG _adjustDpPp (src/BATTLE/BATTLE.PRG/146C.c:10689)
ItemsStatus ConsumeHitEquipmentDp(
    const ItemAction& action, WeaponLoadout& attacker_weapon,
    DefenderEquipment& defender,
    std::span<const std::int16_t, kLimbCount> limb_dp_loss,
    std::int32_t amount) {
  if (!defender.weapon_drawn) return ItemsStatus::kActorOutOfRange;
  if (!action.IsSpell()) {
    attacker_weapon.current_dp = LessDp(attacker_weapon.current_dp, amount);
  }
  if (*defender.weapon_drawn) {
    defender.shield.current_dp = LessDp(defender.shield.current_dp, amount);
  }
  for (std::size_t i = 0; i < kLimbCount; ++i) {
    if (limb_dp_loss[i] == 0) continue;
    auto& armor = defender.limbs[i].armor;
    armor.current_dp = LessDp(armor.current_dp, limb_dp_loss[i]);
  }
  return ItemsStatus::kOk;
}

// decomp: BATTLE.PRG func_800803A4 (src/BATTLE/BATTLE.PRG/146C.c:10721)
ItemsStatus AccumulateHitEquipmentPp(const ItemAction& action,
                                     WeaponLoadout& attacker_weapon,
                                     DefenderEquipment& defender,
                                     std::int32_t dp_amount) {
  if (!defender.weapon_drawn) return ItemsStatus::kActorOutOfRange;
  if (!action.IsSpell()) {
    const auto percent =
        kWholePercent + (HasPpGem(attacker_weapon.gems) ? kPpGemPercent : 0);
    attacker_weapon.current_pp =
        RaisedPp(attacker_weapon.current_pp, attacker_weapon.max_pp,
                 PpGain(dp_amount, percent));
  }
  if (*defender.weapon_drawn) {
    auto& shield = defender.shield;
    const auto percent =
        kWholePercent - (HasPpGem(shield.gems) ? kPpGemPercent : 0);
    shield.current_pp =
        RaisedPp(shield.current_pp, shield.max_pp, PpGain(dp_amount, percent));
  }
  return ItemsStatus::kOk;
}
}  // namespace VS::Items
