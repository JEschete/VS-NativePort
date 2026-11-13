// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionCameraSelectionLive.hpp"

#include <bit>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <vector>

#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionCameraSelection.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/CameraViewLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Sphere.h"
#include "Battle/TargetingState.h"
#include "Core/Diagnostics.h"
#include "Field/CameraLiveState.h"
#include "Render/Camera.h"
}

namespace VS::Battle {
namespace {
[[noreturn]] void Refuse() {
  Core_Errorf("battle",
              "Action camera/selection refused: missing used state, output "
              "or skeleton position, unmasked queue index outside 0..3, "
              "invalid cancel action, or negative wrapped root argument");
  std::abort();
}
void Require(bool valid) {
  if (!valid) Refuse();
}
Battle_ActionState& State() {
  Require(Battle_actionState != nullptr);
  return *Battle_actionState;
}
struct LiveQueue final : ActionCameraRequest {
  explicit LiveQueue(Battle_ActionRequest& value) : record(value) {}
  Battle_ActionRequest& record;
  std::uint8_t PrimaryActor() const override {
    return record.primaryTarget.target.targetActor;
  }
  ActionHit FirstTarget() const override {
    return std::bit_cast<ActionHit>(record.targets[0]);
  }
  std::uint16_t Count() const override { return record.targetCount; }
  ActionPoint Focus() const override {
    return std::bit_cast<ActionPoint>(record.focusPosition);
  }
};
class LiveCamera final : public ActionCameraWorld {
 public:
  ActionCameraRequest* Queued() override {
    auto& state = State();
    Require(state.queueIndex < std::size(state.queue));
    auto& record = state.queue[state.queueIndex];
    for (auto& owner : queues_) {
      if (&owner->record == &record) return owner.get();
    }
    return queues_.emplace_back(std::make_unique<LiveQueue>(record)).get();
  }
  int BoneTag(int actor, int limb) override {
    return Actors::FindLiveActorBoneTag(actor, limb, 2);
  }
  bool BonePosition(int actor, int bone, ActionPoint& output) override {
    auto point = std::bit_cast<SVECTOR>(output);
    // Skeleton's negative result specifically means it produced no position.
    // Its successful result is zero, which the original caller ignored.
    const bool produced =
        Actors::LiveActorBonePosition(actor, bone, &point, 0) >= 0;
    if (produced) output = std::bit_cast<ActionPoint>(point);
    return produced;
  }
  void StepCamera(ActionPoint point) override {
    const auto live = std::bit_cast<SVECTOR>(point);
    constexpr int kTrackingSpeed = 40;
    Field::LiveStepCameraLookAtTowardTarget(&live, 1, kTrackingSpeed);
  }
  Core::Math::Vec3 TrackingDisplacement(ActionPoint point) override {
    const auto live = std::bit_cast<SVECTOR>(point);
    VECTOR step{}, displacement{};
    constexpr int kTrackingSpeed = 40;
    Field::LiveComputeCameraTrackingStep(&live, &step, &displacement, 1,
                                         kTrackingSpeed);
    Core_Debugf("combat-flow", "tracking error=%ld,%ld,%ld", displacement.vx,
                displacement.vy, displacement.vz);
    return Core::Math::VectorOf(displacement);
  }
  std::int32_t CameraMode() override {
    return Field_sphericalCamera.delta.mode;
  }
  std::optional<std::int16_t> Zoom() override {
    auto& state = State();
    Core_Debugf(
        "combat-flow",
        "camera mode=%d zoom=%d yaw=%d distance=%d action=%u targets=%u",
        CameraMode(), state.cameraZoomState, YawDelta(), Distance(),
        static_cast<unsigned>(state.selection.actionId),
        static_cast<unsigned>(state.selection.targetCount));
    return state.cameraZoomState;
  }
  std::int32_t YawDelta() override { return Field_sphericalCamera.delta.yaw; }
  std::int32_t Distance() override {
    return Field_sphericalCamera.values.distance;
  }
  void SetDistance(std::int32_t distance) override {
    Field_sphericalCamera.values.distance = distance;
  }
  std::int32_t Projection() override { return Render_projectionDistance; }
  void SetProjection(std::int32_t projection) override {
    Render_projectionDistance = projection;
  }
  void ApplyProjection(std::int32_t projection) override {
    Render::SetLiveCameraProjectionDistance(projection);
  }
  void StartPresentation() override { StartLiveActionPresentation(); }

 private:
  std::vector<std::unique_ptr<LiveQueue>> queues_;
};
struct LiveSelection final : ActionSelectionState {
  explicit LiveSelection(Battle_ActionState& value) : state(value) {}
  Battle_ActionState& state;
  bool Selecting() const override { return state.isPlayerAction != 0; }
  std::uint8_t SourceActor() const override {
    return state.selection.primaryTarget.target.targetActor;
  }
  std::uint8_t Frames() const override {
    return static_cast<std::uint8_t>(state.selectionFrames);
  }
  void SetFrames(std::uint8_t frames) override {
    state.selectionFrames = static_cast<char>(frames);
  }
  std::uint8_t Preview() const override { return state.cameraPreview; }
  std::uint8_t Selected() const override { return state.selectedPreview; }
  void SetSelected(std::uint8_t selected) override {
    state.selectedPreview = selected;
  }
  std::uint16_t ActionId() const override { return state.selection.actionId; }
  int UpdateMenu() override {
    return LiveUpdateTargetSelectionMenu(&state.cameraPreview);
  }
  int UpdateArea() override {
    return LiveUpdateSphereTargetPosition(&state.areaCenter);
  }
};
class LiveSelectionWorld final : public ActionSelectionWorld {
 public:
  ActionSelectionState* Current() override {
    auto& state = State();
    for (auto& owner : states_) {
      if (&owner->state == &state) return owner.get();
    }
    return states_.emplace_back(std::make_unique<LiveSelection>(state)).get();
  }
  void Commit(int mode) override { CommitLiveSelectedAction(mode); }
  void FilterArea() override { LiveFilterAreaActionTargets(); }
  std::optional<bool> NeedsTargeting(std::uint16_t action) override {
    Require(action < std::size(Battle_actionDefinitions));
    const auto category = Battle_actionDefinitions[action].category;
    return static_cast<std::uint8_t>(
               Battle_categoryNeedsTargetSelection[category]) != 0;
  }
  void InitializeTargeting(std::uint16_t action) override {
    LiveInitializeActionTargeting(action);
  }
  void ClearSphereTargets() override { Battle_ClearSphereTargets(); }
  void ClearPrimaryVolume() override { Battle_ClearPrimarySphereVolume(); }
  void ClearSecondaryVolume() override { Battle_ClearSecondarySphereVolume(); }
  void FreeSphere() override { Battle_FreeSphere(); }
  void ReturnToMenu() override { LiveReturnToBattleActionMenu(); }

 private:
  std::vector<std::unique_ptr<LiveSelection>> states_;
};
}  // namespace

// decomp: func_80077F14 (src/BATTLE/BATTLE.PRG/146C.c:6821)
void LiveActionTargetLimbPosition(int actor, int limb, SVECTOR* output) {
  Require(output != nullptr);
  ActionPoint point{};
  LiveCamera world;
  Require(ActionTargetLimbPosition(actor, limb, point, world));
  output->vx = point.x;
  output->vy = point.y;
  output->vz = point.z;
}
// decomp: func_80077F70 (src/BATTLE/BATTLE.PRG/146C.c:6833)
int LiveActionTargetDistance() {
  LiveCamera world;
  const auto distance = ActionTargetDistance(world);
  if (!distance) Refuse();
  return distance.value();
}
// decomp: func_800780A8 (src/BATTLE/BATTLE.PRG/146C.c:6863)
void LiveActionCameraMidpoint(SVECTOR* output) {
  Require(output != nullptr);
  ActionPoint point{};
  LiveCamera world;
  Require(ActionCameraMidpoint(point, world));
  output->vx = point.x;
  output->vz = point.z;
  output->vy = point.y;
}
// decomp: func_8007820C (src/BATTLE/BATTLE.PRG/146C.c:6886)
void LiveStepCameraTowardActionMidpoint(int execute) {
  LiveCamera world;
  Require(StepCameraTowardActionMidpoint(execute, world));
}
// decomp: func_80078248 (src/BATTLE/BATTLE.PRG/146C.c:6896)
void LiveUpdateInitialTargetSelection() {
  LiveSelectionWorld world;
  Require(UpdateInitialTargetSelection(world));
}
// decomp: func_800782E4 (src/BATTLE/BATTLE.PRG/146C.c:6912)
void LiveUpdateActionAreaSelection() {
  LiveSelectionWorld world;
  Require(UpdateActionAreaSelection(world));
}
// decomp: func_80078364 (src/BATTLE/BATTLE.PRG/146C.c:6927)
void LiveUpdateFinalTargetSelection() {
  LiveSelectionWorld world;
  Require(UpdateFinalTargetSelection(world));
}
// decomp: _handleCombatCameraZoom (src/BATTLE/BATTLE.PRG/146C.c:6958)
void LiveUpdateActionCameraZoom() {
  LiveCamera world;
  const int mode = world.CameraMode();
  if (mode != 1 && mode != kAlternateActionCameraMode) {
    Core_Debugf("combat-flow", "camera mode=%d yaw=%d distance=%d (inactive)",
                mode, world.YawDelta(), world.Distance());
  }
  Require(UpdateActionCameraZoom(world));
}
}  // namespace VS::Battle
