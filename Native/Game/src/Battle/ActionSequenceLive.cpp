// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionSequenceLive.hpp"

#include <cstdlib>
#include <iterator>
#include <memory>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActionSequence.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Items/LootLive.hpp"
#include "Render/CameraViewLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Effects/Runtime.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Field/CameraLiveState.h"
#include "Field/Interactions.h"
#include "Menu/TextBox.h"
#include "Render/Camera.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    Core_Errorf("battle",
                "Action sequence refused: missing used state or invalid "
                "queue/target index or count");
    std::abort();
  }
}
Battle_ActionState& State() {
  Require(Battle_actionState != nullptr);
  return *Battle_actionState;
}
ActionTargetHit Hit(const Battle_HitResult& hit) {
  return {.targets_position = hit.targetsPosition != 0,
          .target_actor = hit.target.targetActor,
          .impact_kind = static_cast<std::uint8_t>(hit.target.impactKind)};
}
class LiveAction final : public SequenceAction {
 public:
  explicit LiveAction(Battle_ActionRequest& action) : action_(action) {}
  ActionTargetHit Primary() const override {
    return Hit(action_.primaryTarget);
  }
  ActionTargetHit Target(std::size_t index) const override {
    Require(index < std::size(action_.targets));
    return Hit(action_.targets[index]);
  }
  std::uint16_t Count() const override {
    Require(action_.targetCount <= std::size(action_.targets));
    return action_.targetCount;
  }
  int RebuildPlans() override {
    return Ai::RebuildLiveActorActionPlans(&action_);
  }

 private:
  Battle_ActionRequest& action_;
};
class LiveWorld final : public ActionSequenceWorld {
 public:
  SequenceAction& Queued() override {
    auto& state = State();
    constexpr std::uint32_t kQueueMask = 3;
    return Bind(state.queue[state.queueIndex & kQueueMask]);
  }
  SequenceAction& Step(std::size_t index) override {
    auto& state = State();
    Require(index < std::size(state.queue));
    return Bind(state.queue[index]);
  }
  std::uint32_t ChainCount() override { return State().chainStep; }
  int NotificationPending() override {
    return Menu_IsSceneNotificationPending();
  }
  std::uint32_t ActorFlags(int actor) override {
    return Actors::LiveActorStateFlags(actor);
  }
  int FreeAssetRequest() override {
    return Content::FindLiveFreeActorAssetRequest();
  }
  int CanStartEffect() override { return Effects_CanStartEffectTrack(); }
  void AdvanceEffectPhase() override { AdvanceLiveActionEffectPhase(); }
  std::uint32_t EffectBusyFlags() override { return Effects_effectBusyFlags; }
  void StartAnimation() override { StartLiveActionAnimation(); }
  void StartEffectTrack() override { Effects_StartEffectTrack(0); }
  void ReleaseAttacks() override { ReleaseLiveQueuedActorAttacks(); }
  void FadeTrails() override { BeginLiveActionTrailFade(); }
  int AnimationBusy(int actor) override {
    return Actors::IsLiveActorActionAnimationBusy(actor);
  }
  int BeginDefeat(int actor) override {
    return BeginLiveActorDefeatAnimation(actor);
  }
  void BeginRecovery(int mode) override { BeginLivePostActionRecovery(mode); }
  std::int32_t CameraMode() override {
    return Field_sphericalCamera.delta.mode;
  }
  std::int32_t Projection() override { return Render_projectionDistance; }
  void ApplyProjection(std::int32_t projection) override {
    Render::SetLiveCameraProjectionDistance(projection);
  }
  std::int32_t Distance() override {
    return Field_sphericalCamera.values.distance;
  }
  void SetDistance(std::int32_t distance) override {
    Field_sphericalCamera.values.distance = distance;
  }
  std::int16_t PreferredDistance() override {
    return State().preferredCameraDistance;
  }
  int LookAtOnAshley() override { return Field::LiveIsCameraLookAtOnAshley(); }
  void ResumeField() override { Field::LiveResumeFieldAfterMenu(); }
  void ShowHud() override { VS::Battle::SetLiveHudHidden(0); }
  std::uint8_t SelectedActor() override {
    return State().selection.primaryTarget.target.targetActor;
  }
  void RestoreIdle(int actor) override {
    Actors::RestoreLiveActorIdlePose(actor);
  }
  void ResolveDefeat(int source, int target) override {
    ResolveLiveActorDefeat(source, target, 1);
  }
  std::uint32_t ControlMode() override { return Field_controlMode; }
  std::int32_t EvaluateRoomEvents() override {
    constexpr int kAfterAction = 12;
    return Event_EvaluateRoomEventTriggersEntry(kAfterAction, 0, nullptr, 0);
  }
  void BeginCutscene() override { Event::BeginLiveQueuedCutscene(); }
  void OpenLoot() override { Items::OpenLivePendingEnemyLoot(); }

 private:
  SequenceAction& Bind(Battle_ActionRequest& action) {
    actions_.push_back(std::make_unique<LiveAction>(action));
    return *actions_.back();
  }
  std::vector<std::unique_ptr<LiveAction>> actions_;
};
}  // namespace

void WaitForLiveActionEffectReady() {
  LiveWorld world;
  WaitForActionEffectReady(world);
}
void StartLiveReadyActionAnimation() {
  LiveWorld world;
  StartReadyActionAnimation(world);
}
void WaitForLiveActionResolution() {
  LiveWorld world;
  WaitForActionResolution(world);
}
void WaitForLiveTargetRecovery() {
  LiveWorld world;
  if (WaitForTargetRecovery(world) == TargetRecoveryStatus::kInvalidSequence) {
    Core_Errorf("battle",
                "Target recovery refused: empty sequence has no last action");
    std::abort();
  }
}
void FinishLiveActionSequence() {
  LiveWorld world;
  FinishActionSequence(world);
}
}  // namespace VS::Battle
