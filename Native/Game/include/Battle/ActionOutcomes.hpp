#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Battle/ActionPreview.hpp"
#include "Battle/ActionResults.hpp"

namespace VS::Battle {
class EnemyDefeatRecord {
 public:
  virtual ~EnemyDefeatRecord() = default;
  virtual std::uint8_t Spawn() const = 0;
  virtual bool Defeated() const = 0;
  virtual void RecordKill() = 0;
  virtual void UpdateWeaponStreak() = 0;
  virtual void QueueLoot() = 0;
  virtual void MarkDefeated() = 0;
};

class ActionOutcomeAction {
 public:
  virtual ~ActionOutcomeAction() = default;
  virtual std::uint16_t Count() const = 0;
  virtual ActionTargetHit Target(std::size_t index) const = 0;
  virtual ActionTargetHit Primary() const = 0;
  // Both services use this request's current fields, not the global queue.
  virtual void DamageReaction(int actor) = 0;
  virtual void ApplyPrimaryResult() = 0;
};
class ActionOutcomeWorld {
 public:
  virtual ~ActionOutcomeWorld() = default;
  virtual ActionOutcomeAction& Queued() = 0;
  virtual ActionPoint Placement(int actor) = 0;
  virtual std::uint8_t WeaponCategory(int actor) = 0;
  virtual void PlaySound(std::uint8_t sound, ActionPoint point) = 0;
  virtual void RecoveryPose() = 0;
};

class ActorDefeatWorld {
 public:
  virtual ~ActorDefeatWorld() = default;
  virtual void RefreshReservations(int actor) = 0;
  virtual bool HasActor(int actor) = 0;
  virtual void SetCommandState(int actor, std::uint8_t state) = 0;
  virtual std::int16_t Hp(int actor) = 0;
  virtual void HideMarker(int actor) = 0;
  virtual bool Retained(int actor) = 0;
  // Uses the current action state's range origin at call entry.
  virtual void BeginDeath(int actor) = 0;
  virtual void LockAnimation(int actor) = 0;
  virtual std::uint8_t Spawn(int actor) = 0;
};

class LearnedActionWorld {
 public:
  virtual ~LearnedActionWorld() = default;
  virtual std::uint8_t LearnedByte(std::size_t index) const = 0;
  virtual void SetUnlocked(std::size_t index, bool unlocked) = 0;
};

void ResolveEnemyDefeat(int source_actor, int count_kill,
                        EnemyDefeatRecord& enemy);
int PlayWeaponHitSounds(int source_actor, ActionOutcomeWorld& world);
void ApplyActionSourceEffects(int source_actor, ActionOutcomeWorld& world);
int BeginActorDefeatAnimation(int actor, ActorDefeatWorld& world);
void ApplyLearnedActionUnlocks(LearnedActionWorld& world);
}  // namespace VS::Battle
