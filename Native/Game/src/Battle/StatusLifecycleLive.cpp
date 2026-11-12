// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusLifecycleLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Battle/StatusLifecycle.hpp"
#include "Core/RandomLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecordsLive.hpp"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("battle", "%s", message);
    std::abort();
  }
}

Battle_ActorStats& StatsOf(Battle_ActorStats* stats) {
  Require(stats != nullptr, "Status lifecycle requires actor stats");
  return *stats;
}

StatusLifecycleState Load(const Battle_ActorStats& stats) {
  StatusLifecycleState state{
      .statuses = std::bit_cast<std::uint32_t>(stats.statuses),
      .hp = stats.currentHp,
      .max_hp = stats.maxHp,
      .strength = {stats.strength, stats.totalStrength},
      .intelligence = {stats.intelligence, stats.totalIntelligence},
      .agility = {stats.agility, stats.totalAgility},
      .base_walk = static_cast<std::uint8_t>(stats.baseWalkSpeed),
      .base_run = static_cast<std::uint8_t>(stats.baseRunSpeed),
      .walk = static_cast<std::uint8_t>(stats.walkSpeed),
      .run = static_cast<std::uint8_t>(stats.runSpeed),
      .mobility_limbs = {static_cast<std::uint8_t>(stats.mobilityLimb0),
                         static_cast<std::uint8_t>(stats.mobilityLimb1)},
      .actor_id = stats.actorId,
      .analyzed = stats.isAnalyzed != 0,
      .weapon = Items::Live::Typed(stats.weapon),
      .shield = Items::Live::Typed(stats.shield)};
  for (std::size_t index = 0; index < state.timers.size(); ++index) {
    state.timers.at(index) = stats.statusTimers[index];
  }
  for (std::size_t index = 0; index < state.limbs.size(); ++index) {
    const auto& limb = stats.limbs[index];
    state.limbs.at(index) = {limb.hp, limb.maxHp, limb.nameIndex,
                             Items::Live::Typed(limb.armor)};
  }
  return state;
}

void Store(const StatusLifecycleState& state, Battle_ActorStats& stats) {
  stats.statuses = std::bit_cast<std::int32_t>(state.statuses);
  stats.currentHp = state.hp;
  stats.maxHp = state.max_hp;
  stats.strength = state.strength.current;
  stats.totalStrength = state.strength.total;
  stats.intelligence = state.intelligence.current;
  stats.totalIntelligence = state.intelligence.total;
  stats.agility = state.agility.current;
  stats.totalAgility = state.agility.total;
  stats.baseWalkSpeed = std::bit_cast<char>(state.base_walk);
  stats.baseRunSpeed = std::bit_cast<char>(state.base_run);
  stats.walkSpeed = std::bit_cast<char>(state.walk);
  stats.runSpeed = std::bit_cast<char>(state.run);
  stats.mobilityLimb0 = state.mobility_limbs.at(0);
  stats.mobilityLimb1 = state.mobility_limbs.at(1);
  stats.actorId = state.actor_id;
  stats.isAnalyzed = state.analyzed;
  Items::Live::Commit(stats.weapon, state.weapon);
  Items::Live::Commit(stats.shield, state.shield);
  for (std::size_t index = 0; index < state.timers.size(); ++index) {
    stats.statusTimers[index] = state.timers.at(index);
  }
  for (std::size_t index = 0; index < state.limbs.size(); ++index) {
    const auto& limb = state.limbs.at(index);
    stats.limbs[index].hp = limb.hp;
    stats.limbs[index].maxHp = limb.max_hp;
    stats.limbs[index].nameIndex = limb.name_index;
    Items::Live::Commit(stats.limbs[index].armor, limb.armor);
  }
}

StatusDurations Durations() {
  constexpr std::size_t kAttributes = 5;
  constexpr std::size_t kMovement = 11;
  constexpr std::size_t kEquipment = 19;
  constexpr std::size_t kParalysis = 13;
  constexpr std::size_t kPoison = 14;
  constexpr std::size_t kRegeneration = 17;
  return {
      static_cast<std::uint8_t>(
          Battle_statusEffectParams[kAttributes].duration),
      static_cast<std::uint8_t>(Battle_statusEffectParams[kMovement].duration),
      static_cast<std::uint8_t>(Battle_statusEffectParams[kEquipment].duration),
      static_cast<std::uint8_t>(Battle_statusEffectParams[kParalysis].duration),
      static_cast<std::uint8_t>(
          Battle_statusEffectParams[kPoison].tickInterval),
      static_cast<std::uint8_t>(
          Battle_statusEffectParams[kRegeneration].tickInterval)};
}

class LiveStatusWorld final : public StatusLifecycleWorld {
 public:
  explicit LiveStatusWorld(Battle_ActorStats& stats) : stats_(stats) {}

  std::int32_t RandomBounded(StatusLifecycleState& state, int bound) override {
    return Call(state, [&] { return Core::NextLiveRandomBounded(bound); });
  }
  std::int32_t RandomAveraged(StatusLifecycleState& state, int bound) override {
    return Call(state, [&] { return Core::NextLiveRandomAveraged(bound); });
  }
  void UpdateMovement(StatusLifecycleState& state) override {
    Require(state.actor_id < std::size(Battle_Ai_actorAiStates),
            "Status movement requires a valid AI slot");
    Call(state, [&] {
      Ai::SetLiveActorVerticalMovementSteps(
          stats_.actorId, static_cast<std::uint8_t>(stats_.walkSpeed),
          static_cast<std::uint8_t>(stats_.runSpeed));
    });
  }
  void EquipmentLabel(StatusLifecycleState& state, int label, int stat,
                      std::uint32_t amount) override {
    Call(state, [&] {
      Battle_QueueEquipmentStatChange(label, stat, std::bit_cast<int>(amount));
    });
  }
  void ValueLabel(StatusLifecycleState& state, std::uint32_t amount) override {
    Call(state,
         [&] { Battle_QueueActorValueLabel(stats_.actorId, 0, amount, 0); });
  }
  void StatusLabels(StatusLifecycleState& state,
                    std::uint32_t statuses) override {
    Call(state, [&] {
      Battle_QueueActorStatusLabels(stats_.actorId, 0,
                                    std::bit_cast<int>(statuses), 1);
    });
  }
  void RebuildWeapon(StatusLifecycleState& state) override {
    Call(state, [&] { Items::RebuildLiveWeaponClassAffinity(&stats_); });
  }
  void RebuildShield(StatusLifecycleState& state) override {
    Call(state, [&] { Items::RebuildLiveShieldClassAffinity(&stats_); });
  }
  void RebuildArmor(StatusLifecycleState& state, std::size_t limb) override {
    Call(state, [&] {
      Items::RebuildLiveArmorClassAffinity(&stats_, static_cast<int>(limb));
    });
  }
  void EnchantWeapon(StatusLifecycleState& state,
                     const ElementEnchantment& change) override {
    Call(state, [&] {
      ApplyLiveWeaponElementEnchantment(&stats_, change.increased_affinity,
                                        change.decreased_affinity,
                                        change.amount);
    });
  }
  void EnchantArmor(StatusLifecycleState& state,
                    const ElementEnchantment& change) override {
    Call(state, [&] {
      ApplyLiveArmorElementEnchantment(&stats_, change.increased_affinity,
                                       change.decreased_affinity,
                                       change.amount);
    });
  }
  void RefreshEquipmentDisplay(StatusLifecycleState& state) override {
    Call(state, [] { Battle_CommitEquipmentStatChanges(); });
  }

 private:
  template <typename Callback>
  std::invoke_result_t<Callback> Call(StatusLifecycleState& state,
                                      Callback callback) {
    Store(state, stats_);
    if constexpr (std::is_void_v<std::invoke_result_t<Callback>>) {
      callback();
      state = Load(stats_);
    } else {
      const auto result = callback();
      state = Load(stats_);
      return result;
    }
  }
  Battle_ActorStats& stats_;
};

template <typename Operation>
void Apply(Battle_ActorStats* pointer, Operation operation) {
  auto& stats = StatsOf(pointer);
  auto state = Load(stats);
  LiveStatusWorld world(stats);
  Require(operation(state, world),
          "Status lifecycle refused a used mobility limb");
  Store(state, stats);
}
}  // namespace

StatusLifecycleState ReadLiveStatusState(const Battle_ActorStats& stats) {
  return Load(stats);
}

void WriteLiveStatusState(const StatusLifecycleState& state,
                          Battle_ActorStats& stats) {
  Store(state, stats);
}

// decomp: func_8007C31C (src/BATTLE/BATTLE.PRG/146C.c:8736).
void PreserveLiveAshleyStatuses() {
  Require(Battle_characterState != nullptr,
          "Preserving statuses requires Ashley");
  Apply(Battle_characterState->stats, PreserveAshleyStatuses);
}

// decomp: func_80086754 (src/BATTLE/BATTLE.PRG/146C.c:13532).
void RemoveLiveActorStatuses(int requested, Battle_ActorStats* stats) {
  Apply(stats, [&](auto& state, auto& world) {
    return RemoveActorStatuses(state, std::bit_cast<std::uint32_t>(requested),
                               world);
  });
}

// decomp: func_80086FA8 (src/BATTLE/BATTLE.PRG/146C.c:13832).
void InflictLiveActorStatuses(int requested, Battle_ActorStats* stats) {
  Apply(stats, [&](auto& state, auto& world) {
    InflictActorStatuses(state, std::bit_cast<std::uint32_t>(requested),
                         Durations(), world);
    return true;
  });
}

// decomp: func_80087EF4 (src/BATTLE/BATTLE.PRG/146C.c:14376).
void UpdateLiveActorStatusTimers(Battle_ActorStats* stats) {
  LimbStatusMasks flags{};
  for (std::size_t index = 0; index < flags.size(); ++index) {
    flags.at(index) =
        std::bit_cast<std::uint32_t>(Battle_limbStatusFlags[index]);
  }
  Apply(stats, [&](auto& state, auto& world) {
    return UpdateActorStatusTimers(state, Durations(), flags, world);
  });
}
}  // namespace VS::Battle
