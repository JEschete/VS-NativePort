// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActorVitalsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <optional>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "State/SceneTimerLive.hpp"
extern "C" {
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
#include "Event/RoomTriggerEntryLive.h"
#include "State/Timers.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("battle", "%s", message);
    std::abort();
  }
}

class LiveVitalsActor final : public VitalsActor {
 public:
  explicit LiveVitalsActor(Battle_Actor& actor) : actor_(actor) {}
  std::uint8_t SpawnIndex() override { return actor_.roomSpawnIndex; }
  std::uint8_t RetainAfterDefeat() override { return actor_.retainAfterDefeat; }
  std::uint32_t WeaponDrawn() override {
    return std::bit_cast<std::uint32_t>(actor_.weaponDrawn);
  }
  ActorVitalsAccess* Stats() override {
    if (actor_.stats == nullptr) return nullptr;
    stats_.emplace(*actor_.stats);
    return &stats_.value();
  }

 private:
  Battle_Actor& actor_;
  std::optional<LiveActorVitalsAccess> stats_;
};

class LiveVitalsWorld final : public ActorVitalsWorld {
 public:
  VitalsActor* ActorAt(int slot) override {
    Require(
        slot >= 0 && static_cast<std::size_t>(slot) < std::size(Battle_actors),
        "Vitals requires a valid actor slot");
    auto* actor = Battle_actors[slot];
    if (actor == nullptr) return nullptr;
    actors_.emplace_back(*actor);
    return &actors_.back();
  }
  std::uint32_t StateFlags(int slot) override {
    return Actors::LiveActorStateFlags(slot);
  }
  std::int32_t ActionAnimationBusy(int slot) override {
    return Actors::IsLiveActorActionAnimationBusy(slot);
  }
  void ResolveEnemyDefeat(int source, int target, int count_kill) override {
    ResolveLiveEnemyDefeat(source, target, count_kill);
  }
  void RestoreIdle(int slot) override {
    Actors::RestoreLiveActorIdlePose(slot);
  }
  void RemoveActor(int slot, int reason) override {
    Actors::RemoveLiveActorAtSlot(slot, reason);
  }
  void SuppressAshleyLabels() override { Battle_SuppressAshleyStatusLabels(); }
  std::int16_t EventTrigger(int event) override {
    return Event_EvaluateRoomEventTriggersEntry(event, 0, nullptr, 0);
  }
  void BeginQueuedCutscene() override { Event::BeginLiveQueuedCutscene(); }
  void BeginDefeatAnimation(int slot) override {
    BeginLiveActorDefeatAnimation(slot);
  }
  void StatusTimers(ActorVitalsAccess& stats) override {
    auto& owner = static_cast<LiveActorVitalsAccess&>(stats).Owner();
    UpdateLiveActorStatusTimers(&owner);
  }
  std::int32_t Counter() override { return Battle_vitalsTickCounter; }
  void SetCounter(std::int32_t value) override {
    Battle_vitalsTickCounter = value;
  }
  std::int32_t Ticks() override { return State_tickSpeed; }
  std::int32_t RegeneratingLimb() override { return Battle_regeneratingLimb; }
  void SetRegeneratingLimb(std::int32_t value) override {
    Battle_regeneratingLimb = value;
  }
  int SceneTimer() override { return State::UpdateLiveSceneTimer(); }

 private:
  // Stable handles preserve the pre-callback actor, without writing it back or
  // touching it after a defeat callback may have freed/replaced the table slot.
  std::deque<LiveVitalsActor> actors_;
};

int Checked(std::optional<int> result) {
  if (!result) {
    Core_Errorf("battle", "Vitals refused missing stats/actor or a used limb");
    std::abort();
  }
  return result.value();
}
}  // namespace

LiveActorVitalsAccess::LiveActorVitalsAccess(Battle_ActorStats& stats)
    : stats_(stats) {}

ActorVitalsState LiveActorVitalsAccess::Read() {
  return {ReadLiveStatusState(stats_), stats_.currentMp, stats_.maxMp,
          stats_.risk, stats_.actionRecoveryTimer};
}

void LiveActorVitalsAccess::Write(const ActorVitalsState& state) {
  WriteLiveStatusState(state.status, stats_);
  stats_.currentMp = state.mp;
  stats_.maxMp = state.max_mp;
  stats_.risk = state.risk;
  stats_.actionRecoveryTimer = state.recovery;
}

Battle_ActorStats& LiveActorVitalsAccess::Owner() { return stats_; }

// decomp: func_800792E4 (src/BATTLE/BATTLE.PRG/146C.c:7413).
int ResolveLiveActorDefeat(int source, int target, int count_kill) {
  LiveVitalsWorld world;
  return Checked(ResolveActorDefeat(source, target, count_kill, world));
}

// decomp: func_80088554 (src/BATTLE/BATTLE.PRG/146C.c:14560).
int UpdateLiveActorVitals() {
  LiveVitalsWorld world;
  return Checked(UpdateActorVitals(world));
}

// decomp: func_80088B6C (src/BATTLE/BATTLE.PRG/146C.c:14743).
int UpdateLiveRoomActorVitals() {
  LiveVitalsWorld world;
  return Checked(UpdateRoomActorVitals(world));
}
}  // namespace VS::Battle
