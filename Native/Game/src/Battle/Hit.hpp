#pragma once
// The queued outcome of an action on one target: the stat changes, statuses,
// per-stat effect directions, limb results and equipment wear that the action
// sequence applies when the hit lands.

#include <cstddef>
#include <cstdint>

#include "Battle/ActionResolution.hpp"
#include "Battle/BattleTypes.hpp"

namespace VS::Battle {

inline constexpr int kHitLimbCount = 6;
inline constexpr std::size_t kHitResultBytes = 0x44;

// One limb's hit result: its two effect values, or both read as one word.
// decomp: the limbs union of HitEntity (src/BATTLE/BATTLE.PRG/hitEntity.h:47)
union HitLimbResult {
  VS::Battle::LimbEffects effects;
  std::uint32_t packed;
};

// decomp: _hitEntity_t, upstream HitEntity
// (src/BATTLE/BATTLE.PRG/hitEntity.h:56)
struct HitResult {
  HitTargetWord target;
  std::int16_t risk_change;
  std::int16_t strength_change;
  std::int16_t intelligence_change;
  std::int16_t agility_change;
  std::int16_t max_hp_change;
  std::int16_t max_mp_change;
  std::int32_t remove_statuses;
  std::int32_t inflict_statuses;
  std::uint32_t hp_effect : 2;
  std::uint32_t mp_effect : 2;
  std::uint32_t risk_effect : 2;
  std::uint32_t strength_effect : 2;
  std::uint32_t intelligence_effect : 2;
  std::uint32_t agility_effect : 2;
  std::uint32_t max_hp_effect : 2;
  std::uint32_t max_mp_effect : 2;
  std::uint32_t stat_status_effect : 2;
  std::uint32_t weapon_dp_effect : 2;
  std::uint32_t weapon_pp_effect : 2;
  std::uint32_t armor_dp_effect : 2;
  std::uint32_t shield_pp_effect : 2;
  std::uint32_t source_result : 2;
  std::uint32_t reserved_bits28 : 4;
  HitLimbResult limbs[kHitLimbCount];
  std::int16_t weapon_dp_change;
  std::int16_t weapon_pp_change;
  std::int16_t armor_dp_change;
  std::int16_t shield_pp_change;
  std::int32_t targets_position;
};
static_assert(sizeof(HitResult) == kHitResultBytes,
              "HitResult must keep its PS1 layout");

}  // namespace VS::Battle
