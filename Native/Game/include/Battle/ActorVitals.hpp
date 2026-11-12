#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/StatusLifecycle.hpp"

namespace VS::Battle {
struct ActorVitalsState {
  StatusLifecycleState status{};
  std::int16_t mp{};
  std::int16_t max_mp{};
  std::uint16_t risk{};
  std::uint16_t recovery{};
  friend bool operator==(const ActorVitalsState&,
                         const ActorVitalsState&) = default;
};

// A stats owner stays bound even if a callback replaces its actor/table entry.
class ActorVitalsAccess {
 public:
  virtual ~ActorVitalsAccess() = default;
  virtual ActorVitalsState Read() = 0;
  virtual void Write(const ActorVitalsState& state) = 0;
};

class VitalsActor {
 public:
  virtual ~VitalsActor() = default;
  virtual std::uint8_t SpawnIndex() = 0;
  virtual std::uint8_t RetainAfterDefeat() = 0;
  virtual std::uint32_t WeaponDrawn() = 0;
  virtual ActorVitalsAccess* Stats() = 0;
};

class ActorVitalsWorld {
 public:
  virtual ~ActorVitalsWorld() = default;
  virtual VitalsActor* ActorAt(int slot) = 0;
  virtual std::uint32_t StateFlags(int slot) = 0;
  virtual std::int32_t ActionAnimationBusy(int slot) = 0;
  virtual void ResolveEnemyDefeat(int source, int target, int count_kill) = 0;
  virtual void RestoreIdle(int slot) = 0;
  virtual void RemoveActor(int slot, int reason) = 0;
  virtual void SuppressAshleyLabels() = 0;
  virtual std::int16_t EventTrigger(int event) = 0;
  virtual void BeginQueuedCutscene() = 0;
  virtual void BeginDefeatAnimation(int slot) = 0;
  virtual void StatusTimers(ActorVitalsAccess& stats) = 0;
  virtual std::int32_t Counter() = 0;
  virtual void SetCounter(std::int32_t value) = 0;
  virtual std::int32_t Ticks() = 0;
  virtual std::int32_t RegeneratingLimb() = 0;
  virtual void SetRegeneratingLimb(std::int32_t value) = 0;
  virtual int SceneTimer() = 0;
};

// Empty actors are normal; missing required stats or a post-defeat actor,
// and an invalid limb actually used for regeneration, refuse with nullopt.
// decomp: func_800792E4 (src/BATTLE/BATTLE.PRG/146C.c:7413).
std::optional<int> ResolveActorDefeat(int source, int target, int count_kill,
                                      ActorVitalsWorld& world);
// decomp: func_80088554 (src/BATTLE/BATTLE.PRG/146C.c:14560).
std::optional<int> UpdateActorVitals(ActorVitalsWorld& world);
// decomp: func_80088B6C (src/BATTLE/BATTLE.PRG/146C.c:14743).
std::optional<int> UpdateRoomActorVitals(ActorVitalsWorld& world);
}  // namespace VS::Battle
