// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionSequenceLive.hpp"

#include <cstdlib>
#include <iterator>
#include <memory>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActionSequence.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/CombatActionLive.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/Runtime.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/Interactions.hpp"
#include "Items/LootLive.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"

using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf("battle",
                     "Action sequence refused: missing used state or invalid "
                     "queue/target index or count");
    std::abort();
  }
}
ActionState& State() {
  Require(VS::Battle::actionState != nullptr);
  return *VS::Battle::actionState;
}
ActionTargetHit Hit(const VS::Battle::HitResult& hit) {
  return {.targets_position = hit.targets_position != 0,
          .target_actor = hit.target.target_actor,
          .impact_kind = static_cast<std::uint8_t>(hit.target.impact_kind)};
}
class LiveAction final : public SequenceAction {
 public:
  explicit LiveAction(ActionRequestRecord& action) : action_(action) {
    if (const auto* result =
            FindLiveCombatAction(LiveCombatActionForRequest(action))) {
      native_ = *result;
    }
  }
  ActionTargetHit Primary() const override {
    if (native_) return LiveCombatTarget(native_->source);
    return Hit(action_.primary_target);
  }
  ActionTargetHit Target(std::size_t index) const override {
    if (native_) return LiveCombatTarget(native_->hits.at(index));
    Require(index < std::size(action_.targets));
    return Hit(action_.targets[index]);
  }
  std::uint16_t Count() const override {
    if (native_) return static_cast<std::uint16_t>(native_->hits.size());
    Require(action_.target_count <= std::size(action_.targets));
    return action_.target_count;
  }
  int RebuildPlans() override {
    if (native_) return Ai::RebuildLiveCombatActionPlans(*native_);
    return Ai::RebuildLiveActorActionPlans(&action_);
  }

 private:
  ActionRequestRecord& action_;
  std::optional<CombatActionResult> native_;
};
class LiveWorld final : public ActionSequenceWorld {
 public:
  LiveWorld() {
    if (actionState == nullptr) return;
    for (const auto& request : actionState->queue) {
      const auto id = LiveCombatActionForRequest(request);
      if (id != CombatActionId::kNone) pending_.push_back(id);
    }
  }
  void RetireFinishedActions() const {
    if (!finished_) return;
    for (const auto id : pending_) ForgetLiveCombatAction(id);
  }
  SequenceAction& Queued() override {
    auto& state = State();
    constexpr std::uint32_t kQueueMask = 3;
    return Bind(state.queue[state.queue_index & kQueueMask]);
  }
  SequenceAction& Step(std::size_t index) override {
    auto& state = State();
    Require(index < std::size(state.queue));
    return Bind(state.queue[index]);
  }
  std::uint32_t ChainCount() override { return State().chain_step; }
  int NotificationPending() override {
    return Menu::IsLiveSceneNotificationPending();
  }
  std::uint32_t ActorFlags(int actor) override {
    return Actors::LiveActorStateFlags(actor);
  }
  int FreeAssetRequest() override {
    return Content::FindLiveFreeActorAssetRequest();
  }
  int CanStartEffect() override { return Effects::LiveCanStartEffectTrack(); }
  void AdvanceEffectPhase() override { AdvanceLiveActionEffectPhase(); }
  std::uint32_t EffectBusyFlags() override {
    return VS::Effects::effectBusyFlags;
  }
  void StartAnimation() override { StartLiveActionAnimation(); }
  void StartEffectTrack() override { VS::Effects::LiveStartEffectTrack(0); }
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
    return VS::Field::sphericalCamera.motion.zoom_mode;
  }
  std::int32_t Projection() override { return VS::Render::projectionDistance; }
  void ApplyProjection(std::int32_t projection) override {
    Render::SetLiveCameraProjectionDistance(projection);
  }
  std::int32_t Distance() override {
    return VS::Field::sphericalCamera.values.distance;
  }
  void SetDistance(std::int32_t distance) override {
    VS::Field::sphericalCamera.values.distance = distance;
  }
  std::int16_t PreferredDistance() override {
    return State().preferred_camera_distance;
  }
  int LookAtOnAshley() override { return Field::LiveIsCameraLookAtOnAshley(); }
  void ResumeField() override {
    finished_ = true;
    Field::LiveResumeFieldAfterMenu();
  }
  void ShowHud() override { VS::Battle::SetLiveHudHidden(0); }
  std::uint8_t SelectedActor() override {
    const auto id = LiveCombatActionForRequest(State().queue.front());
    if (const auto* result = FindLiveCombatAction(id)) {
      return static_cast<std::uint8_t>(
          LiveCombatTarget(result->source).target_actor);
    }
    return State().selection.primary_target.target.target_actor;
  }
  void RestoreIdle(int actor) override {
    const auto id = LiveCombatActionForRequest(State().queue.front());
    if (const auto* result = FindLiveCombatAction(id)) {
      if (LiveCombatTarget(result->source).targets_position ||
          result->source.changes.target != actor) {
        return;
      }
    }
    Actors::RestoreLiveActorIdlePose(actor);
  }
  void ResolveDefeat(int source, int target) override {
    ResolveLiveActorDefeat(source, target, 1);
  }
  std::uint32_t ControlMode() override { return VS::Field::controlMode; }
  std::int32_t EvaluateRoomEvents() override {
    constexpr int kAfterAction = 12;
    return VS::Event::EvaluateRoomEventTriggersEntry(kAfterAction, 0, nullptr,
                                                     0);
  }
  void BeginCutscene() override { Event::BeginLiveQueuedCutscene(); }
  void OpenLoot() override { Items::OpenLivePendingEnemyLoot(); }

 private:
  SequenceAction& Bind(ActionRequestRecord& action) {
    actions_.push_back(std::make_unique<LiveAction>(action));
    return *actions_.back();
  }
  std::vector<std::unique_ptr<LiveAction>> actions_;
  std::vector<CombatActionId> pending_;
  bool finished_{};
};
}  // namespace

void WaitForLiveActionEffectReady() {
  if (!LiveCombatPresentationAvailable(CurrentLiveCombatAction())) {
    BeginLivePostActionRecovery(0);
    return;
  }
  LiveWorld world;
  WaitForActionEffectReady(world);
}
void StartLiveReadyActionAnimation() {
  if (!LiveCombatPresentationAvailable(CurrentLiveCombatAction())) {
    BeginLivePostActionRecovery(0);
    return;
  }
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
    VS::Core::Errorf(
        "battle", "Target recovery refused: empty sequence has no last action");
    std::abort();
  }
}
void FinishLiveActionSequence() {
  LiveWorld world;
  FinishActionSequence(world);
  world.RetireFinishedActions();
}
}  // namespace VS::Battle
