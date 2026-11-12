// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusHelpers.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
constexpr std::uint8_t kSuppressedNumberSpawn = 128;
constexpr std::uint32_t kRiskControlMode = 2;
constexpr std::uint32_t kRiskFlowLimit = 4;
constexpr std::uint32_t kTargetingRiskStatus = 8;
constexpr std::uint16_t kTargetingRiskLimit = 100;
constexpr std::int32_t kRiskPeriod = 30;

bool ValidAffinities(const ElementEnchantment& change) {
  const auto valid = [](int index) {
    return index >= 0 &&
           static_cast<std::size_t>(index) < Items::kPartAffinitySlots;
  };
  if (valid(change.increased_affinity) && valid(change.decreased_affinity)) {
    return true;
  }
  Core_Debugf("battle",
              "Element enchantment indices %d/%d are outside their tables",
              change.increased_affinity, change.decreased_affinity);
  return false;
}

std::int16_t LowHalf(std::uint32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

void ApplyEnchantment(Items::ClassAffinityCurrent& affinity,
                      Items::AffinityTotals& modifiers,
                      const ElementEnchantment& change) {
  auto& increased = affinity.affinities.at(0).at(change.increased_affinity);
  const auto amount = static_cast<std::uint32_t>(change.amount);
  increased =
      LowHalf(std::uint32_t{std::bit_cast<std::uint16_t>(increased)} + amount);
  // Read after the first store: both indices may name the same element.
  auto& decreased = affinity.affinities.at(0).at(change.decreased_affinity);
  decreased =
      LowHalf(std::uint32_t{std::bit_cast<std::uint16_t>(decreased)} - amount);
  modifiers.at(change.increased_affinity) = LowHalf(amount);
  modifiers.at(change.decreased_affinity) = LowHalf(0U - amount);
}
}  // namespace

// decomp: func_80086EA4 (src/BATTLE/BATTLE.PRG/146C.c:13801).
bool ApplyWeaponElementEnchantment(Items::WeaponLoadout& weapon,
                                   const ElementEnchantment& change) {
  if (weapon.blade.id == 0) return true;
  if (!ValidAffinities(change)) return false;
  ApplyEnchantment(weapon.class_affinity, weapon.affinity_modifiers, change);
  return true;
}

// decomp: func_80086EF4 (src/BATTLE/BATTLE.PRG/146C.c:13811).
bool ApplyArmorElementEnchantment(
    Items::ShieldLoadout& shield,
    std::span<Items::ArmorLoadout, Items::kLimbCount> armor,
    const ElementEnchantment& change) {
  if (shield.base.id == 0 && std::ranges::none_of(armor, [](const auto& limb) {
        return limb.armor.id != 0;
      })) {
    return true;
  }
  if (!ValidAffinities(change)) return false;
  if (shield.base.id != 0) {
    ApplyEnchantment(shield.class_affinity, shield.affinity_modifiers, change);
  }
  for (auto& limb : armor) {
    if (limb.armor.id != 0) {
      ApplyEnchantment(limb.class_affinity, limb.affinity_modifiers, change);
    }
  }
  return true;
}

// decomp: func_80087578 (src/BATTLE/BATTLE.PRG/146C.c:14011).
std::optional<bool> QueueActorStatChangeNumber(
    StatNumberWorld& world, const StatNumberRequest& request) {
  const auto spawn = world.RoomSpawnIndex(request.actor_slot);
  if (!spawn) {
    Core_Debugf("battle", "Stat number requires actor slot %d",
                request.actor_slot);
    return std::nullopt;
  }
  if (spawn.value() == kSuppressedNumberSpawn && request.actor_slot != 0) {
    return false;
  }
  world.QueueValueLabel(request);
  return true;
}

// decomp: func_80088B8C (src/BATTLE/BATTLE.PRG/146C.c:14745).
bool UpdateTargetingRiskPenalty(std::int32_t& timer,
                                TargetingRiskWorld& world) {
  if (world.ControlMode() != kRiskControlMode) return true;
  const auto flow = world.ActionFlowMode();
  if (flow == 0 || flow >= kRiskFlowLimit) return true;
  if (timer == 0) {
    const auto target = world.SelectedTarget();
    if (!target) {
      Core_Debugf("battle",
                  "Targeting risk requires the selected actor's stats");
      return false;
    }
    if ((target.value().statuses & kTargetingRiskStatus) != 0 &&
        !target.value().targets_position && target.value().actor_slot == 0) {
      const auto risk = world.PlayerRisk();
      if (!risk) {
        Core_Debugf("battle", "Targeting risk requires the player's stats");
        return false;
      }
      if (risk.value() < kTargetingRiskLimit) {
        world.SetPlayerRisk(static_cast<std::uint16_t>(risk.value() + 1));
      }
    }
  }
  const auto advanced = std::bit_cast<std::uint32_t>(timer) +
                        std::bit_cast<std::uint32_t>(world.TickSpeed());
  timer = std::bit_cast<std::int32_t>(advanced) % kRiskPeriod;
  return true;
}

// decomp: func_80088CA0 (src/BATTLE/BATTLE.PRG/146C.c:14764).
void ResetTargetingRiskPenaltyTimer(std::int32_t& timer) { timer = 0; }
}  // namespace VS::Battle
