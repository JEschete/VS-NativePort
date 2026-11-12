// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusLifecycle.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Items/EquipmentAffinity.hpp"

namespace VS::Battle {
namespace {
constexpr std::uint32_t kCripple = 0x10;
constexpr std::uint32_t kQuicken = 0x800;
constexpr std::uint32_t kPoison = 0x4000;
constexpr std::uint32_t kHalfSpeed = 0x8010;
constexpr std::uint32_t kCurse = 0x10000;
constexpr std::uint32_t kRegeneration = 0x20000;
constexpr std::uint32_t kEquipmentDown = 0x80000;
constexpr std::uint32_t kEquipmentUp = 0x100000;
constexpr std::uint32_t kWeaponElements = 0x1E00000;
constexpr std::uint32_t kArmorElements = 0x1E000000;
constexpr std::uint32_t kAnalyze = 0x20000000;
constexpr std::uint32_t kNegativeLabel = 0x80000000;
constexpr int kAttributeDivisor = 10;
constexpr int kGearPercent = 7;
constexpr int kPercentDivisor = 100;
constexpr int kGrowthLimit = 50;
constexpr int kGrowthDraw = 8;
constexpr int kDecayDraw = 16;
constexpr int kHpDivisor = 20;
constexpr int kPoisonDraw = 6;
constexpr int kRegenerationDraw = 5;
constexpr int kLimbHealingDivisor = 8;
constexpr int kRecoveredHp = 2;
constexpr std::size_t kHealthTimer = 5;
constexpr int kAir = 1;
constexpr int kFire = 2;
constexpr int kEarth = 3;
constexpr int kWater = 4;
constexpr auto kTimerStatuses = std::to_array<std::uint32_t>(
    {0x7E0, 0x810, 0x1FF80000, 0x2000, 0x8000, 0x24000});

std::int16_t Halfword(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

bool ValidMobility(const StatusLifecycleState& state) {
  return std::ranges::all_of(state.mobility_limbs, [&](auto index) {
    return index < state.limbs.size();
  });
}

std::optional<ElementEnchantment> ElementPair(std::uint32_t normalized) {
  switch (normalized) {
    case 1U << (kAir - 1):
      return ElementEnchantment{kAir, kEarth};
    case 1U << (kFire - 1):
      return ElementEnchantment{kFire, kWater};
    case 1U << (kEarth - 1):
      return ElementEnchantment{kEarth, kAir};
    case 1U << (kWater - 1):
      return ElementEnchantment{kWater, kFire};
    default:
      return std::nullopt;
  }
}

void RestoreAttributes(StatusLifecycleState& state, std::uint32_t requested) {
  constexpr std::array<std::uint32_t, 3> kAttributeMasks{0x60, 0x180, 0x600};
  const std::array attributes{&state.strength, &state.intelligence,
                              &state.agility};
  for (std::size_t index = 0; index < attributes.size(); ++index) {
    if ((requested & (kAttributeMasks.at(index) | kCurse)) != 0) {
      attributes.at(index)->current = attributes.at(index)->total;
    }
  }
}

void RestoreMovement(StatusLifecycleState& state, std::uint32_t requested,
                     StatusLifecycleWorld& world) {
  if ((requested & kHalfSpeed) != 0) {
    if ((requested & kCripple) != 0) {
      for (const auto index : state.mobility_limbs) {
        if (index == 0) continue;
        auto& limb = state.limbs.at(index);
        if (limb.max_hp != 0 && limb.hp < kRecoveredHp) limb.hp = kRecoveredHp;
      }
    }
    if ((state.statuses & kCripple) == 0) {
      state.walk = state.base_walk;
      state.run = state.base_run;
    }
    world.UpdateMovement(state);
  }
  if ((requested & kQuicken) != 0 && (state.statuses & kHalfSpeed) == 0) {
    state.walk = state.base_walk;
    state.run = state.base_run;
    world.UpdateMovement(state);
  }
}

template <typename Operation>
void EquippedAttributes(StatusLifecycleState& state, Operation operation) {
  if (state.weapon.blade.id != 0) operation(state.weapon.attributes);
  if (state.shield.base.id != 0) operation(state.shield.attributes);
  for (auto& limb : state.limbs) {
    if (limb.armor.armor.id != 0) operation(limb.armor.attributes);
  }
}

template <typename Label>
void GrowElement(StatusLifecycleState& state, Items::EquipmentPart& part,
                 Label label, const ElementEnchantment& pair,
                 StatusLifecycleWorld& world) {
  if (part.id == 0) return;
  const auto increase = static_cast<std::size_t>(pair.increased_affinity);
  const auto decrease = static_cast<std::size_t>(pair.decreased_affinity);
  if (part.affinities.at(increase) < kGrowthLimit &&
      world.RandomBounded(state, kGrowthDraw) == 0) {
    part.affinities.at(increase) =
        static_cast<std::int8_t>(part.affinities.at(increase) + 1);
    if (state.actor_id == 0) {
      world.EquipmentLabel(
          state, label(),
          pair.increased_affinity + static_cast<int>(Items::kClassCount), 1);
    }
  }
  if (part.affinities.at(decrease) > -Items::kRatingLimit &&
      world.RandomBounded(state, kDecayDraw) == 0) {
    part.affinities.at(decrease) =
        static_cast<std::int8_t>(part.affinities.at(decrease) - 1);
    if (state.actor_id == 0) {
      world.EquipmentLabel(
          state, label(),
          pair.decreased_affinity + static_cast<int>(Items::kClassCount),
          kNegativeLabel | 1U);
    }
  }
}

void ClearModifiers(Items::AffinityTotals& modifiers,
                    const ElementEnchantment& pair) {
  modifiers.at(static_cast<std::size_t>(pair.increased_affinity)) = 0;
  modifiers.at(static_cast<std::size_t>(pair.decreased_affinity)) = 0;
}

void RemoveElements(StatusLifecycleState& state, std::uint32_t requested,
                    StatusLifecycleWorld& world) {
  constexpr int kWeaponShift = 21;
  constexpr int kArmorShift = 25;
  if (const auto pair =
          ElementPair((requested & kWeaponElements) >> kWeaponShift)) {
    GrowElement(
        state, state.weapon.blade, [] { return Items::kWeaponNoticeLabel; },
        pair.value(), world);
    ClearModifiers(state.weapon.affinity_modifiers, pair.value());
    world.RebuildWeapon(state);
  }
  if ((requested & kArmorElements) == 0) return;
  if (const auto pair =
          ElementPair((requested & kArmorElements) >> kArmorShift)) {
    GrowElement(
        state, state.shield.base, [] { return Items::kShieldNoticeLabel; },
        pair.value(), world);
    ClearModifiers(state.shield.affinity_modifiers, pair.value());
    world.RebuildShield(state);
    for (std::size_t index = 0; index < state.limbs.size(); ++index) {
      auto& limb = state.limbs.at(index);
      GrowElement(
          state, limb.armor.armor, [&limb] { return limb.name_index; },
          pair.value(), world);
      ClearModifiers(limb.armor.affinity_modifiers, pair.value());
      world.RebuildArmor(state, index);
    }
  }
  world.RefreshEquipmentDisplay(state);
}

void InflictAttributes(StatusLifecycleState& state, std::uint32_t requested) {
  constexpr std::array<std::uint32_t, 3> kDownMasks{0x20, 0x80, 0x200};
  const std::array attributes{&state.strength, &state.intelligence,
                              &state.agility};
  for (std::size_t index = 0; index < attributes.size(); ++index) {
    auto& attribute = *attributes.at(index);
    const auto down = kDownMasks.at(index);
    if ((requested & down) != 0) {
      attribute.current = static_cast<std::uint16_t>(
          attribute.total - attribute.total / kAttributeDivisor);
    }
    if ((requested & (down << 1U)) != 0) {
      attribute.current = static_cast<std::uint16_t>(
          attribute.total + attribute.total / kAttributeDivisor);
    }
  }
  if ((requested & kCurse) != 0) {
    for (auto* attribute : attributes) {
      attribute->current = static_cast<std::uint16_t>(
          attribute->total - attribute->total / kAttributeDivisor);
    }
  }
}

void InflictMovement(StatusLifecycleState& state, std::uint32_t requested,
                     StatusLifecycleWorld& world) {
  if ((requested & (kHalfSpeed | kQuicken)) == 0) return;
  if ((state.statuses & kHalfSpeed) != 0) {
    state.walk = state.base_walk >> 1U;
    state.run = state.base_run >> 1U;
  } else if ((state.statuses & kQuicken) != 0) {
    state.walk =
        static_cast<std::uint8_t>(state.base_walk + (state.base_walk >> 2U));
    state.run =
        static_cast<std::uint8_t>(state.base_run + (state.base_run >> 2U));
  }
  world.UpdateMovement(state);
}

void InflictEquipment(StatusLifecycleState& state, std::uint32_t requested,
                      StatusLifecycleWorld& world) {
  for (const auto mask : {kEquipmentDown, kEquipmentUp}) {
    if ((requested & mask) == 0) continue;
    const int amount =
        std::max(0, (state.intelligence.current +
                     state.weapon.attributes.current_intelligence) *
                        kGearPercent / kPercentDivisor);
    EquippedAttributes(state, [&](auto& attributes) {
      attributes.current_strength =
          Halfword(attributes.current_strength +
                   (mask == kEquipmentDown ? -amount : amount));
    });
  }
  if ((requested & kAnalyze) != 0) state.analyzed = true;
  constexpr int kWeaponShift = 21;
  constexpr int kArmorShift = 25;
  if (auto pair = ElementPair((requested & kWeaponElements) >> kWeaponShift)) {
    pair.value().amount =
        std::max(0, (state.intelligence.current +
                     state.weapon.attributes.current_intelligence) /
                        2);
    world.EnchantWeapon(state, pair.value());
  }
  if (auto pair = ElementPair((requested & kArmorElements) >> kArmorShift)) {
    pair.value().amount =
        std::max(0, (state.intelligence.current +
                     state.weapon.attributes.current_intelligence) /
                        2);
    world.EnchantArmor(state, pair.value());
  }
}

void TickHealth(StatusLifecycleState& state, const StatusDurations& durations,
                StatusLifecycleWorld& world) {
  if ((state.statuses & kPoison) != 0) {
    state.timers.at(kHealthTimer) = durations.poison_interval;
    const int draw = world.RandomBounded(state, kPoisonDraw);
    const int damage = std::max(1, state.hp / kHpDivisor + draw);
    state.hp = state.hp < damage ? 0 : Halfword(state.hp - damage);
    world.ValueLabel(state,
                     static_cast<std::uint32_t>(damage) | kNegativeLabel);
  } else if ((state.statuses & kRegeneration) != 0) {
    state.timers.at(kHealthTimer) = durations.regeneration_interval;
    const int draw = world.RandomAveraged(state, kRegenerationDraw);
    const int healing = state.max_hp / kHpDivisor - 2 + draw;
    // SH precedes signed LH/comparison: wrapping is before the upper clamp.
    state.hp = std::min(Halfword(state.hp + healing), state.max_hp);
    for (auto& limb : state.limbs) {
      if (limb.max_hp != 0) {
        limb.hp = std::min(Halfword(limb.hp + healing / kLimbHealingDivisor),
                           limb.max_hp);
      }
    }
    world.ValueLabel(state, static_cast<std::uint32_t>(healing));
  }
}

std::uint32_t RecoveredStatuses(const StatusLifecycleState& state,
                                const LimbStatusMasks& limb_statuses) {
  std::uint32_t recovered = 0;
  if ((state.statuses & kCripple) != 0) {
    for (const auto index : state.mobility_limbs) {
      if (index != 0 && state.limbs.at(index).hp >= kRecoveredHp) {
        recovered |= kCripple;
      }
    }
  }
  constexpr std::uint32_t kPlayerLimbStatuses = 0xF;
  if (state.actor_id == 0 && (state.statuses & kPlayerLimbStatuses) != 0 &&
      state.mobility_limbs.at(0) != 0) {
    for (std::size_t index = 0; index < limb_statuses.size(); ++index) {
      if (state.limbs.at(index).hp >= kRecoveredHp) {
        recovered |= state.statuses & limb_statuses.at(index);
      }
    }
  }
  return recovered;
}
}  // namespace

// decomp: func_8007C31C (src/BATTLE/BATTLE.PRG/146C.c:8736).
bool PreserveAshleyStatuses(StatusLifecycleState& state,
                            StatusLifecycleWorld& world) {
  return RemoveActorStatuses(state, 0, world);
}

// decomp: func_80086754 (src/BATTLE/BATTLE.PRG/146C.c:13532).
bool RemoveActorStatuses(StatusLifecycleState& state, std::uint32_t requested,
                         StatusLifecycleWorld& world) {
  if ((requested & kCripple) != 0 && !ValidMobility(state)) return false;
  state.statuses &= ~requested;
  for (std::size_t index = 0; index < kTimerStatuses.size(); ++index) {
    const auto bucket = kTimerStatuses.at(index);
    if ((requested & bucket) != 0 && (state.statuses & bucket) == 0) {
      state.timers.at(index) = 0;
    }
  }
  RestoreAttributes(state, requested);
  RestoreMovement(state, requested, world);
  if ((requested & (kEquipmentDown | kEquipmentUp)) != 0) {
    EquippedAttributes(state, [](auto& attributes) {
      attributes.current_strength = Halfword(attributes.base_strength);
      attributes.current_intelligence = Halfword(attributes.base_intelligence);
    });
  }
  RemoveElements(state, requested, world);
  return true;
}

// decomp: func_80086FA8 (src/BATTLE/BATTLE.PRG/146C.c:13832).
void InflictActorStatuses(StatusLifecycleState& state, std::uint32_t requested,
                          const StatusDurations& durations,
                          StatusLifecycleWorld& world) {
  state.statuses |= requested;
  const std::array times{durations.attributes, durations.movement,
                         durations.equipment, durations.paralysis,
                         durations.paralysis};
  for (std::size_t index = 0; index < times.size(); ++index) {
    if ((requested & kTimerStatuses.at(index)) != 0) {
      state.timers.at(index) = times.at(index);
    }
  }
  if ((requested & kPoison) != 0) {
    state.timers.at(kHealthTimer) = durations.poison_interval;
  } else if ((requested & kRegeneration) != 0) {
    state.timers.at(kHealthTimer) = durations.regeneration_interval;
  }
  InflictAttributes(state, requested);
  InflictMovement(state, requested, world);
  InflictEquipment(state, requested, world);
}

// decomp: func_80087EF4 (src/BATTLE/BATTLE.PRG/146C.c:14376).
bool UpdateActorStatusTimers(StatusLifecycleState& state,
                             const StatusDurations& durations,
                             const LimbStatusMasks& limb_statuses,
                             StatusLifecycleWorld& world) {
  if ((state.statuses & kCripple) != 0 && !ValidMobility(state)) return false;
  std::uint32_t expired = 0;
  for (std::size_t index = 0; index < kTimerStatuses.size(); ++index) {
    if (state.timers.at(index) == 0) continue;
    --state.timers.at(index);
    if (state.timers.at(index) != 0) continue;
    if (index < kTimerStatuses.size() - 1) {
      expired |= state.statuses & kTimerStatuses.at(index);
    } else {
      TickHealth(state, durations, world);
    }
  }
  if ((state.statuses & kCripple) != 0 && !ValidMobility(state)) return false;
  expired |= RecoveredStatuses(state, limb_statuses);
  if (expired != 0) {
    world.StatusLabels(state, expired);
    return RemoveActorStatuses(state, expired, world);
  }
  return true;
}
}  // namespace VS::Battle
