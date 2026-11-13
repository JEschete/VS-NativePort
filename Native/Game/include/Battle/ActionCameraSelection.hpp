#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/ActionPreview.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
inline constexpr std::int32_t kAlternateActionCameraMode = 3;
class ActionCameraRequest {
 public:
  virtual ~ActionCameraRequest() = default;
  virtual std::uint8_t PrimaryActor() const = 0;
  virtual std::uint16_t Count() const = 0;
  virtual ActionHit FirstTarget() const = 0;
  virtual ActionPoint Focus() const = 0;
};
class ActionCameraWorld {
 public:
  virtual ~ActionCameraWorld() = default;
  // The queued request remains owned by its original state across callbacks.
  virtual ActionCameraRequest* Queued() = 0;
  virtual int BoneTag(int actor, int limb) = 0;
  // False means no position was produced, not a zero-valued helper result.
  virtual bool BonePosition(int actor, int bone, ActionPoint& output) = 0;
  virtual void StepCamera(ActionPoint point) = 0;
  virtual Core::Math::Vec3 TrackingDisplacement(ActionPoint point) = 0;
  virtual std::int32_t CameraMode() = 0;
  virtual std::optional<std::int16_t> Zoom() = 0;
  virtual std::int32_t YawDelta() = 0;
  virtual std::int32_t Distance() = 0;
  virtual void SetDistance(std::int32_t distance) = 0;
  virtual std::int32_t Projection() = 0;
  virtual void SetProjection(std::int32_t projection) = 0;
  virtual void ApplyProjection(std::int32_t projection) = 0;
  virtual void StartPresentation() = 0;
};

class ActionSelectionState {
 public:
  virtual ~ActionSelectionState() = default;
  virtual bool Selecting() const = 0;
  virtual std::uint8_t SourceActor() const = 0;
  virtual std::uint8_t Frames() const = 0;
  virtual void SetFrames(std::uint8_t frames) = 0;
  virtual std::uint8_t Preview() const = 0;
  virtual std::uint8_t Selected() const = 0;
  virtual void SetSelected(std::uint8_t selected) = 0;
  virtual std::uint16_t ActionId() const = 0;
  // Services receive this owner's fields, not a reloaded global owner.
  virtual int UpdateMenu() = 0;
  virtual int UpdateArea() = 0;
};
class ActionSelectionWorld {
 public:
  virtual ~ActionSelectionWorld() = default;
  virtual ActionSelectionState* Current() = 0;
  virtual void Commit(int mode) = 0;
  virtual void FilterArea() = 0;
  virtual std::optional<bool> NeedsTargeting(std::uint16_t action) = 0;
  virtual void InitializeTargeting(std::uint16_t action) = 0;
  virtual void ClearSphereTargets() = 0;
  virtual void ClearPrimaryVolume() = 0;
  virtual void ClearSecondaryVolume() = 0;
  virtual void FreeSphere() = 0;
  virtual void ReturnToMenu() = 0;
};

bool ActionTargetLimbPosition(int actor, int limb, ActionPoint& output,
                              ActionCameraWorld& world);
// Negative wrapped root arguments address outside the original root table.
// Such inputs have no supported numerical result; they are not clamped to 0.
std::optional<std::int32_t> ActionTargetDistance(ActionCameraWorld& world);
bool ActionCameraMidpoint(ActionPoint& output, ActionCameraWorld& world);
bool StepCameraTowardActionMidpoint(int execute, ActionCameraWorld& world);
bool UpdateInitialTargetSelection(ActionSelectionWorld& world);
bool UpdateActionAreaSelection(ActionSelectionWorld& world);
bool UpdateFinalTargetSelection(ActionSelectionWorld& world);
bool UpdateActionCameraZoom(ActionCameraWorld& world);
}  // namespace VS::Battle
