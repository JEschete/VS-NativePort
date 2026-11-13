#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Battle/ActionResults.hpp"

namespace VS::Battle {
class SequenceAction {
 public:
  virtual ~SequenceAction() = default;
  virtual ActionTargetHit Primary() const = 0;
  virtual ActionTargetHit Target(std::size_t index) const = 0;
  virtual std::uint16_t Count() const = 0;
  virtual int RebuildPlans() = 0;
};

class ActionSequenceWorld {
 public:
  virtual ~ActionSequenceWorld() = default;
  // Owners survive callbacks; fields and the global sequence are read afresh.
  virtual SequenceAction& Queued() = 0;
  virtual SequenceAction& Step(std::size_t index) = 0;
  virtual std::uint32_t ChainCount() = 0;
  virtual int NotificationPending() = 0;
  virtual std::uint32_t ActorFlags(int actor) = 0;
  virtual int FreeAssetRequest() = 0;
  virtual int CanStartEffect() = 0;
  virtual void AdvanceEffectPhase() = 0;
  virtual std::uint32_t EffectBusyFlags() = 0;
  virtual void StartAnimation() = 0;
  virtual void StartEffectTrack() = 0;
  virtual void ReleaseAttacks() = 0;
  virtual void FadeTrails() = 0;
  virtual int AnimationBusy(int actor) = 0;
  virtual int BeginDefeat(int actor) = 0;
  virtual void BeginRecovery(int mode) = 0;
  virtual std::int32_t CameraMode() = 0;
  virtual std::int32_t Projection() = 0;
  // Commits the projection before calling the renderer.
  virtual void ApplyProjection(std::int32_t projection) = 0;
  virtual std::int32_t Distance() = 0;
  virtual void SetDistance(std::int32_t distance) = 0;
  virtual std::int16_t PreferredDistance() = 0;
  virtual int LookAtOnAshley() = 0;
  virtual void ResumeField() = 0;
  virtual void ShowHud() = 0;
  virtual std::uint8_t SelectedActor() = 0;
  virtual void RestoreIdle(int actor) = 0;
  virtual void ResolveDefeat(int source, int target) = 0;
  virtual std::uint32_t ControlMode() = 0;
  virtual std::int32_t EvaluateRoomEvents() = 0;
  virtual void BeginCutscene() = 0;
  virtual void OpenLoot() = 0;
};

enum class TargetRecoveryStatus { kWaiting, kStarted, kInvalidSequence };

void WaitForActionEffectReady(ActionSequenceWorld& world);
void StartReadyActionAnimation(ActionSequenceWorld& world);
void WaitForActionResolution(ActionSequenceWorld& world);
// An initially empty sequence has no last request and is an invalid phase.
// A callback may empty a nonempty sequence without invalidating its last owner.
TargetRecoveryStatus WaitForTargetRecovery(ActionSequenceWorld& world);
void FinishActionSequence(ActionSequenceWorld& world);
}  // namespace VS::Battle
