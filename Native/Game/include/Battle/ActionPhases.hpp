#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorCommands.hpp"
#include "Field/GameMode.hpp"

namespace VS::Battle {
enum class ActionPhase : std::uint32_t {
  kInitial = 0,
  kAbilityFinished = 9,
  kTrailFade = 10,
  kRecovery = 11
};

class ActionPhaseSource {
 public:
  virtual ~ActionPhaseSource() = default;
  virtual std::uint8_t Actor() const = 0;
};

class ActionPhaseWorld {
 public:
  virtual ~ActionPhaseWorld() = default;
  virtual std::optional<Actors::ActorCommand> Command(int slot) = 0;
  virtual void SetCommandState(int slot, std::uint8_t state) = 0;
  virtual std::uint8_t PrimaryActor() = 0;
  virtual std::uint32_t ActorFlags(int slot) = 0;
  virtual void RestoreIdle(int slot) = 0;
  virtual void RestoreRoomPalettes() = 0;
  virtual void FreezeAnimations() = 0;
  virtual void EnableClutAnimation() = 0;
  virtual void ResumeRoomAnimations() = 0;
  virtual void ShowActorMarkers() = 0;
  virtual void SetRecoveryReward(std::int32_t mode) = 0;
  virtual void SetTickSpeed(std::uint32_t speed) = 0;
  virtual void SetPhase(ActionPhase phase) = 0;
  virtual void SetControlMode(Field::FieldControlMode mode) = 0;
  virtual int TrailBone(std::uint8_t actor) = 0;
  virtual void FadeTrails(std::uint8_t actor, int bone) = 0;
  virtual void ToggleAshleyWeapon() = 0;
};

void ReleaseQueuedActorAttacks(ActionPhaseWorld& world);
void WaitForPostActionMovement(ActionPhaseWorld& world);
void BeginPostActionRecovery(std::int32_t reward_mode, ActionPhaseWorld& world);
// The source retains queue[0]'s owner, not a copy of its mutable actor field.
void BeginActionTrailFade(const ActionPhaseSource& source,
                          ActionPhaseWorld& world);
void FinishBattleAbilitySequence(ActionPhaseWorld& world);
void ResetActionPhase(ActionPhaseWorld& world);
void BeginAshleyWeaponToggle(ActionPhaseWorld& world);
}  // namespace VS::Battle
