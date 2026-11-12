// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActorVitals.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle {
namespace {
constexpr int kRoomSlots = 16;
constexpr std::uint32_t kDefeated = 0x01000000;
constexpr std::uint32_t kActiveMask = 0x01002001;
constexpr std::uint8_t kUnspawned = 128;
constexpr int kRecoveryStep = 35;
constexpr int kBaseInterval = 30;
constexpr int kStatusInterval = 60;
constexpr int kRestInterval = 120;
constexpr int kDrawnMpInterval = 180;
constexpr int kDrawnHpInterval = 240;
constexpr int kCycle = 720;
constexpr std::uint32_t kSlowRiskStatus = 8;
constexpr int kEnemyDefeatTrigger = 12;
constexpr int kAshleyDefeatTrigger = 64;

std::int32_t WrappedAdd(std::int32_t left, std::int32_t right) {
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                     static_cast<std::uint32_t>(right));
}

void RecoverMp(ActorVitalsState& state, VitalsActor& actor) {
  if (state.mp < state.max_mp) ++state.mp;
  if ((actor.WeaponDrawn() & 1U) != 0) {
    if (state.status.weapon.current_pp != 0) --state.status.weapon.current_pp;
    if (state.status.shield.current_pp != 0) --state.status.shield.current_pp;
  }
}

bool RecoverHp(ActorVitalsState& state, std::int32_t limb_index) {
  if (limb_index < 0 ||
      static_cast<std::size_t>(limb_index) >= state.status.limbs.size()) {
    return false;
  }
  if (state.status.hp != 0 && state.status.hp < state.status.max_hp) {
    ++state.status.hp;
  }
  auto& limb = state.status.limbs.at(static_cast<std::size_t>(limb_index));
  if (limb.max_hp != 0 && limb.hp < limb.max_hp) ++limb.hp;
  return true;
}

bool Regenerate(ActorVitalsState& state, VitalsActor& actor,
                ActorVitalsWorld& world) {
  if (world.Counter() % kBaseInterval != 0 || state.status.hp == 0) {
    return true;
  }
  const bool drawn = (actor.WeaponDrawn() & 1U) != 0;
  const bool slow_risk =
      drawn || (state.status.statuses & kSlowRiskStatus) != 0;
  if ((!slow_risk || world.Counter() % kRestInterval == 0) && state.risk != 0) {
    --state.risk;
  }
  if (world.Counter() % (drawn ? kDrawnMpInterval : kStatusInterval) == 0) {
    RecoverMp(state, actor);
  }
  return world.Counter() % (drawn ? kDrawnHpInterval : kRestInterval) != 0 ||
         RecoverHp(state, world.RegeneratingLimb());
}

std::optional<int> ResolveRoomDefeat(int slot, ActorVitalsWorld& world) {
  auto result = ResolveActorDefeat(0, slot, 0, world);
  if (result == 0 && slot != 0) {
    result = world.EventTrigger(kEnemyDefeatTrigger) == 1 ? 1 : 2;
  }
  return result;
}

std::optional<int> UpdateSlot(int slot, VitalsActor& actor,
                              ActorVitalsWorld& world) {
  if (actor.SpawnIndex() == kUnspawned && slot != 0) return 0;
  const auto flags = world.StateFlags(slot);
  if ((flags & kDefeated) != 0) {
    return world.ActionAnimationBusy(slot) == 1
               ? std::optional<int>{0}
               : ResolveRoomDefeat(slot, world);
  }
  if ((flags & kActiveMask) != 1) return 0;
  auto* stats = actor.Stats();
  if (stats == nullptr) return 0;
  auto state = stats->Read();
  state.recovery =
      state.recovery > kRecoveryStep
          ? static_cast<std::uint16_t>(state.recovery - kRecoveryStep)
          : 0;
  if (!Regenerate(state, actor, world)) return std::nullopt;
  stats->Write(state);
  if (world.Counter() % kStatusInterval != 0) return 0;
  if (state.status.hp != 0) world.StatusTimers(*stats);
  if (stats->Read().status.hp != 0) return 0;
  if (actor.RetainAfterDefeat() != 0) return ResolveRoomDefeat(slot, world);
  world.BeginDefeatAnimation(slot);
  return 0;
}
}  // namespace

// decomp: func_800792E4 (src/BATTLE/BATTLE.PRG/146C.c:7413).
std::optional<int> ResolveActorDefeat(int source, int target, int count_kill,
                                      ActorVitalsWorld& world) {
  auto* actor = world.ActorAt(target);
  if (actor == nullptr) return 0;
  auto* stats = actor->Stats();
  if (stats == nullptr) return std::nullopt;
  if (stats->Read().status.hp != 0) {
    world.RestoreIdle(target);
    return 0;
  }
  world.ResolveEnemyDefeat(source, target, count_kill);
  if (target == 0) {
    world.SuppressAshleyLabels();
    world.EventTrigger(kAshleyDefeatTrigger);
    world.BeginQueuedCutscene();
    return 1;
  }
  actor = world.ActorAt(target);
  if (actor == nullptr) return std::nullopt;
  if (actor->RetainAfterDefeat() == 0) world.RemoveActor(target, 1);
  return 0;
}

// decomp: func_80088554 (src/BATTLE/BATTLE.PRG/146C.c:14560).
std::optional<int> UpdateActorVitals(ActorVitalsWorld& world) {
  int outcome = 0;
  for (int slot = 0; slot < kRoomSlots; ++slot) {
    auto* actor = world.ActorAt(slot);
    if (actor == nullptr) continue;
    const auto result = UpdateSlot(slot, *actor, world);
    if (!result) return std::nullopt;
    if (outcome == 0 || result == 1) outcome = result.value();
  }
  world.SetCounter(WrappedAdd(world.Counter(), world.Ticks()) % kCycle);
  if (world.Counter() % kDrawnHpInterval == 0) {
    world.SetRegeneratingLimb(WrappedAdd(world.RegeneratingLimb(), 1) %
                              static_cast<int>(Items::kLimbCount));
  }
  const int timer_result = world.SceneTimer();
  return timer_result != 0 ? timer_result : outcome;
}

// decomp: func_80088B6C (src/BATTLE/BATTLE.PRG/146C.c:14743).
std::optional<int> UpdateRoomActorVitals(ActorVitalsWorld& world) {
  return UpdateActorVitals(world);
}
}  // namespace VS::Battle
