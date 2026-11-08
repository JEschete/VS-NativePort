#pragma once

#include <cstdint>
#include <optional>

#include "Field/GameMode.hpp"
#include "Field/InteractionTarget.hpp"

namespace VS::Field {

struct ChestLockState {
  std::int16_t tile_x = 0;
  std::int16_t tile_z = 0;
  std::int16_t lock_level = 0;
  std::int16_t required_item = 0;
  std::int16_t opened_flag = 0;
};

enum class ChestSelectionResult { kPending, kCancelled, kAccepted };

class ChestInteractionHost {
 public:
  virtual ~ChestInteractionHost() = default;
  virtual std::optional<ChestLockState> Chest(ChestTarget target) const = 0;
  virtual bool ScriptFlag(std::int32_t flag) = 0;
  virtual bool HasItem(std::int16_t item) = 0;
  virtual void SetScriptFlag(std::int32_t flag) = 0;
  virtual void ConsumeItem(std::int16_t item) = 0;
  virtual bool Unlock(ChestTarget target) = 0;
  virtual void ShowUnlocked(std::int16_t item) = 0;
  virtual void PlayPose(const ChestLockState& chest, std::uint8_t pose) = 0;
  virtual bool StartAnimation(ChestTarget target) = 0;
  virtual bool SelectChest(ChestTarget, std::int32_t) = 0;
  virtual std::optional<ChestTarget> SelectedChest() const = 0;
  virtual void BeginChestFlow() = 0;
  virtual void FinishChestFlow() = 0;
  virtual void RestoreIdlePoses() = 0;
  virtual void ResetActionSelection() = 0;
  virtual void ReleaseNotification() = 0;
  virtual void ShowLocked(std::int32_t, std::int16_t) = 0;
  virtual void OpenChestLoot(ChestTarget) = 0;
  virtual void OpenChestStorage(std::int32_t) = 0;
  virtual void PauseForDialog() = 0;
  virtual void PauseForMenu() = 0;
  virtual bool NotificationPending() = 0;
  virtual ChestSelectionResult UpdateContentsSelection() = 0;
  virtual void ReleaseContentsRows() = 0;
  virtual void ResumeField() = 0;
};

bool TryOpenChest(ChestTarget target, ChestInteractionHost& host);
bool BeginChestInteraction(ChestTarget target, std::int32_t actor,
                           ChestInteractionHost& host);
bool FinishChestInteraction(ChestInteractionHost& host);
bool OpenChestContents(ChestInteractionHost& host);
bool UpdateChestContentsSelection(ChestInteractionHost& host);

class InteractionDispatchHost {
 public:
  virtual ~InteractionDispatchHost() = default;
  virtual InteractionTarget Target() const = 0;
  virtual bool TrapMode() const = 0;
  virtual void BeginDispatch() = 0;
  virtual bool DispatchTrap(TrapTarget target) = 0;
  virtual bool DispatchChest(ChestTarget target) = 0;
};

bool DispatchTrapOrChestInteraction(InteractionDispatchHost& host);

class TrapFinishHost {
 public:
  virtual ~TrapFinishHost() = default;
  virtual std::optional<TrapTarget> Target() const = 0;
  virtual bool DisableTrap(TrapTarget target) = 0;
  virtual void FinishFlow() = 0;
  virtual void ReleaseNotification() = 0;
  virtual void ClearTarget() = 0;
};

bool FinishTrapInteraction(TrapFinishHost& host);

class TrapSelectionHost {
 public:
  virtual ~TrapSelectionHost() = default;
  virtual bool Valid(TrapTarget target, std::int32_t actor) const = 0;
  virtual void BeginFlow() = 0;
  virtual void Select(TrapTarget target, std::int32_t actor) = 0;
  virtual void RestoreIdle(std::int32_t actor) = 0;
  virtual void ShowInformation(TrapTarget target) = 0;
  virtual void ResetActionSelection() = 0;
};

bool BeginTrapInteraction(TrapTarget target, std::int32_t actor,
                          TrapSelectionHost& host);

struct TrapPosition {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  friend bool operator==(const TrapPosition&, const TrapPosition&) = default;
};

struct TrapActionInput {
  std::int32_t actor = -1;
  std::uint8_t default_limb = 0;
  TrapPosition position{};
};

inline constexpr std::uint8_t kTrapGroundAnchor = 0xFD;

struct TrapEffectRequest {
  std::uint16_t effect_id = 0;
  std::int32_t actor = -1;
  std::uint8_t ground_anchor = kTrapGroundAnchor;
  std::optional<TrapPosition> source_position{};
};

class TrapActionHost {
 public:
  virtual ~TrapActionHost() = default;
  virtual bool HasActionState(const TrapActionInput& input) const = 0;
  virtual std::optional<std::int16_t> Kind(TrapTarget target) const = 0;
  virtual bool StageVictim(const TrapActionInput& input) = 0;
  virtual bool ResolveAction(std::uint16_t action_id) = 0;
  virtual bool BeginEffect(const TrapEffectRequest& request) = 0;
};

bool TriggerActorTrap(TrapTarget target, const TrapActionInput& input,
                      TrapActionHost& host);

class InteractionCameraHost {
 public:
  virtual ~InteractionCameraHost() = default;
  virtual std::optional<TrapPosition> Focus() const = 0;
  virtual std::optional<TrapPosition> AshleyPosition() const = 0;
  virtual std::int32_t StepLookAt(const TrapPosition& point, std::int32_t mode,
                                  std::int32_t speed) = 0;
  virtual std::uint32_t AshleyStateFlags() const = 0;
  virtual bool NotificationPending() = 0;
  virtual bool AshleyTransitionPose() = 0;
};

bool WaitForChestCameraFocus(InteractionCameraHost& host);
std::optional<std::int32_t> StepTrapCameraFocus(InteractionCameraHost& host);
bool IsChestAnimationReady(InteractionCameraHost& host);
std::optional<std::int32_t> TrackInteractionCameraToAshley(
    InteractionCameraHost& host);

class ObjectInteractionWaitHost {
 public:
  virtual ~ObjectInteractionWaitHost() = default;
  virtual void SetControlMode(FieldControlMode mode) = 0;
  virtual void PauseField() = 0;
  virtual void ReleaseNotification() = 0;
  virtual void ResumeField() = 0;
  virtual void EnterExploration() = 0;
};

void BeginObjectInteractionWait(ObjectInteractionWaitHost& host);
void EndObjectInteractionWait(ObjectInteractionWaitHost& host);

struct RoomObjectLockState {
  std::int16_t tile_x = 0;
  std::int16_t tile_z = 0;
  std::int16_t required_item = 0;
  std::uint8_t lock_state = 0;
  bool is_open = false;
  bool is_triggered = false;
};

enum class RoomObjectInteractionResult { kWait, kBusy, kRefused };

class RoomObjectInteractionHost {
 public:
  virtual ~RoomObjectInteractionHost() = default;
  virtual std::optional<RoomObjectLockState> State() const = 0;
  virtual bool HasItem(std::int16_t item) = 0;
  virtual bool PreviousUnlockFlag(std::int32_t flag) = 0;
  virtual void SetUnlockFlag(std::int32_t flag, std::int16_t value) = 0;
  virtual void ConsumeItem(std::int16_t item) = 0;
  virtual bool Unlock() = 0;
  virtual void ShowNotification(std::int32_t kind, std::int16_t item) = 0;
  virtual void PlayPose(const RoomObjectLockState& state,
                        std::uint8_t pose) = 0;
  virtual void BeginToggle() = 0;
  virtual void PlaySound(std::int32_t sound) = 0;
  virtual void RestoreIdle(std::int32_t actor) = 0;
};

RoomObjectInteractionResult InteractWithLockedRoomObject(
    RoomObjectInteractionHost& host);

}  // namespace VS::Field
