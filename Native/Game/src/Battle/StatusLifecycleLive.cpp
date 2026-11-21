// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusLifecycleLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <type_traits>

#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Battle/StatusLifecycle.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/RandomLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecordsLive.hpp"

namespace VS::Battle {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("battle", "%s", message);
    std::abort();
  }
}

VS::Battle::ActorStats& StatsOf(VS::Battle::ActorStats* stats) {
  Require(stats != nullptr, "Status lifecycle requires actor stats");
  return *stats;
}

StatusLifecycleState Load(const VS::Battle::ActorStats& stats) {
  StatusLifecycleState state{
      .statuses = std::bit_cast<std::uint32_t>(stats.statuses),
      .hp = stats.current_hp,
      .max_hp = stats.max_hp,
      .strength = {stats.strength, stats.total_strength},
      .intelligence = {stats.intelligence, stats.total_intelligence},
      .agility = {stats.agility, stats.total_agility},
      .base_walk = static_cast<std::uint8_t>(stats.base_walk_speed),
      .base_run = static_cast<std::uint8_t>(stats.base_run_speed),
      .walk = static_cast<std::uint8_t>(stats.walk_speed),
      .run = static_cast<std::uint8_t>(stats.run_speed),
      .mobility_limbs = {static_cast<std::uint8_t>(stats.mobility_limb0),
                         static_cast<std::uint8_t>(stats.mobility_limb1)},
      .actor_id = stats.actor_id,
      .analyzed = stats.is_analyzed != 0,
      .weapon = stats.weapon,
      .shield = stats.shield};
  for (std::size_t index = 0; index < state.timers.size(); ++index) {
    state.timers.at(index) = stats.status_timers[index];
  }
  for (std::size_t index = 0; index < state.limbs.size(); ++index) {
    const auto& limb = stats.limbs[index];
    state.limbs.at(index) = {limb.hp, limb.max_hp, limb.name_index, limb.armor};
  }
  return state;
}

void Store(const StatusLifecycleState& state, VS::Battle::ActorStats& stats) {
  stats.statuses = std::bit_cast<std::int32_t>(state.statuses);
  stats.current_hp = state.hp;
  stats.max_hp = state.max_hp;
  stats.strength = state.strength.current;
  stats.total_strength = state.strength.total;
  stats.intelligence = state.intelligence.current;
  stats.total_intelligence = state.intelligence.total;
  stats.agility = state.agility.current;
  stats.total_agility = state.agility.total;
  stats.base_walk_speed = std::bit_cast<char>(state.base_walk);
  stats.base_run_speed = std::bit_cast<char>(state.base_run);
  stats.walk_speed = std::bit_cast<char>(state.walk);
  stats.run_speed = std::bit_cast<char>(state.run);
  stats.mobility_limb0 = state.mobility_limbs.at(0);
  stats.mobility_limb1 = state.mobility_limbs.at(1);
  stats.actor_id = state.actor_id;
  stats.is_analyzed = state.analyzed;
  stats.weapon = state.weapon;
  stats.shield = state.shield;
  for (std::size_t index = 0; index < state.timers.size(); ++index) {
    stats.status_timers[index] = state.timers.at(index);
  }
  for (std::size_t index = 0; index < state.limbs.size(); ++index) {
    const auto& limb = state.limbs.at(index);
    stats.limbs[index].hp = limb.hp;
    stats.limbs[index].max_hp = limb.max_hp;
    stats.limbs[index].name_index = limb.name_index;
    stats.limbs[index].armor = limb.armor;
  }
}

StatusDurations Durations() {
  constexpr std::size_t kAttributes = 5;
  constexpr std::size_t kMovement = 11;
  constexpr std::size_t kEquipment = 19;
  constexpr std::size_t kParalysis = 13;
  constexpr std::size_t kPoison = 14;
  constexpr std::size_t kRegeneration = 17;
  return {static_cast<std::uint8_t>(
              VS::Battle::statusEffectParams[kAttributes].duration),
          static_cast<std::uint8_t>(
              VS::Battle::statusEffectParams[kMovement].duration),
          static_cast<std::uint8_t>(
              VS::Battle::statusEffectParams[kEquipment].duration),
          static_cast<std::uint8_t>(
              VS::Battle::statusEffectParams[kParalysis].duration),
          static_cast<std::uint8_t>(
              VS::Battle::statusEffectParams[kPoison].tick_interval),
          static_cast<std::uint8_t>(
              VS::Battle::statusEffectParams[kRegeneration].tick_interval)};
}

class LiveStatusWorld final : public StatusLifecycleWorld {
 public:
  explicit LiveStatusWorld(VS::Battle::ActorStats& stats) : stats_(stats) {}

  std::int32_t RandomBounded(StatusLifecycleState& state, int bound) override {
    return Call(state, [&] { return Core::NextLiveRandomBounded(bound); });
  }
  std::int32_t RandomAveraged(StatusLifecycleState& state, int bound) override {
    return Call(state, [&] { return Core::NextLiveRandomAveraged(bound); });
  }
  void UpdateMovement(StatusLifecycleState& state) override {
    Require(state.actor_id < std::size(VS::Battle::Ai::actorAiStates),
            "Status movement requires a valid AI slot");
    Call(state, [&] {
      Ai::SetLiveActorVerticalMovementSteps(
          stats_.actor_id, static_cast<std::uint8_t>(stats_.walk_speed),
          static_cast<std::uint8_t>(stats_.run_speed));
    });
  }
  void EquipmentLabel(StatusLifecycleState& state, int label, int stat,
                      std::uint32_t amount) override {
    Call(state, [&] {
      LiveQueueEquipmentStatChange(label, stat, std::bit_cast<int>(amount));
    });
  }
  void ValueLabel(StatusLifecycleState& state, std::uint32_t amount) override {
    Call(state,
         [&] { LiveQueueActorValueLabel(stats_.actor_id, 0, amount, 0); });
  }
  void StatusLabels(StatusLifecycleState& state,
                    std::uint32_t statuses) override {
    Call(state, [&] {
      LiveQueueActorStatusLabels(stats_.actor_id, 0,
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
    Call(state, [] { LiveCommitEquipmentStatChanges(); });
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
  VS::Battle::ActorStats& stats_;
};

template <typename Operation>
void Apply(VS::Battle::ActorStats* pointer, Operation operation) {
  auto& stats = StatsOf(pointer);
  auto state = Load(stats);
  LiveStatusWorld world(stats);
  Require(operation(state, world),
          "Status lifecycle refused a used mobility limb");
  Store(state, stats);
}
}  // namespace

StatusLifecycleState ReadLiveStatusState(const VS::Battle::ActorStats& stats) {
  return Load(stats);
}

void WriteLiveStatusState(const StatusLifecycleState& state,
                          VS::Battle::ActorStats& stats) {
  Store(state, stats);
}

// decomp: func_8007C31C (src/BATTLE/BATTLE.PRG/146C.c:8687).
void PreserveLiveAshleyStatuses() {
  Require(VS::Battle::characterState != nullptr,
          "Preserving statuses requires Ashley");
  Apply(VS::Battle::characterState->stats, PreserveAshleyStatuses);
}

// decomp: func_80086754 (src/BATTLE/BATTLE.PRG/146C.c:13532).
void RemoveLiveActorStatuses(int requested, VS::Battle::ActorStats* stats) {
  Apply(stats, [&](auto& state, auto& world) {
    return RemoveActorStatuses(state, std::bit_cast<std::uint32_t>(requested),
                               world);
  });
}

// decomp: func_80086FA8 (src/BATTLE/BATTLE.PRG/146C.c:13832).
void InflictLiveActorStatuses(int requested, VS::Battle::ActorStats* stats) {
  Apply(stats, [&](auto& state, auto& world) {
    InflictActorStatuses(state, std::bit_cast<std::uint32_t>(requested),
                         Durations(), world);
    return true;
  });
}

// decomp: func_80087EF4 (src/BATTLE/BATTLE.PRG/146C.c:14243).
void UpdateLiveActorStatusTimers(VS::Battle::ActorStats* stats) {
  LimbStatusMasks flags{};
  for (std::size_t index = 0; index < flags.size(); ++index) {
    flags.at(index) =
        std::bit_cast<std::uint32_t>(VS::Battle::limbStatusFlags[index]);
  }
  Apply(stats, [&](auto& state, auto& world) {
    return UpdateActorStatusTimers(state, Durations(), flags, world);
  });
}
}  // namespace VS::Battle
