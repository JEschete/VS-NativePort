// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPhasesLive.hpp"

#include <bit>
#include <cstdlib>
#include <iterator>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionPhases.hpp"
#include "Battle/BattleModeLive.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Diagnostics.h"
#include "Effects/ActionEffects.h"
#include "Field/Interactions.h"
#include "State/Progress.h"
#include "State/Timers.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    Core_Errorf("battle", "Action phase refused: missing used state or actor");
    std::abort();
  }
}
Battle_ActionState& State() {
  Require(Battle_actionState != nullptr);
  return *Battle_actionState;
}
Battle_Actor* Actor(int slot) {
  Require(slot >= 0 &&
          static_cast<std::size_t>(slot) < std::size(Battle_actors));
  return Battle_actors[slot];
}
struct LiveSource final : ActionPhaseSource {
  explicit LiveSource(Battle_ActionRequest& action) : action_(action) {}
  std::uint8_t Actor() const override {
    return action_.primaryTarget.target.targetActor;
  }

 private:
  Battle_ActionRequest& action_;
};
class LiveWorld final : public ActionPhaseWorld {
 public:
  std::optional<Actors::ActorCommand> Command(int slot) override {
    const auto* actor = Actor(slot);
    if (actor == nullptr) return std::nullopt;
    return Actors::ActorCommand{.state = actor->commandState,
                                .opcode = actor->commandOpcode};
  }
  void SetCommandState(int slot, std::uint8_t state) override {
    auto* actor = Actor(slot);
    Require(actor != nullptr);
    actor->commandState = state;
  }
  std::uint8_t PrimaryActor() override {
    return State().selection.primaryTarget.target.targetActor;
  }
  std::uint32_t ActorFlags(int slot) override {
    return Actors::LiveActorStateFlags(slot);
  }
  void RestoreIdle(int slot) override {
    Actors::RestoreLiveActorIdlePose(slot);
  }
  void RestoreRoomPalettes() override {
    constexpr auto kColors = Field::kResumeSceneColorTransform;
    Render::TransformLiveRoomPalettes(kColors.mode, kColors.duration,
                                      kColors.red_offset, kColors.green_offset,
                                      kColors.blue_offset);
  }
  void FreezeAnimations() override { Actors::FreezeLiveActorAnimations(); }
  void EnableClutAnimation() override {
    Render::SetLiveClutAnimationEnabled(1);
  }
  void ResumeRoomAnimations() override {
    Field::LiveSetRoomAnimationsPaused(0);
  }
  void ShowActorMarkers() override { VS::Battle::SetLiveActorMarkersHidden(0); }
  void SetRecoveryReward(std::int32_t mode) override {
    Battle_recoveryRewardMode = mode;
  }
  void SetTickSpeed(std::uint32_t speed) override {
    State_tickSpeed = std::bit_cast<std::int32_t>(speed);
  }
  void SetPhase(ActionPhase phase) override {
    Battle_actionFlowMode = static_cast<std::uint32_t>(phase);
  }
  void SetControlMode(Field::FieldControlMode mode) override {
    Field_controlMode = static_cast<std::uint32_t>(mode);
  }
  int TrailBone(std::uint8_t actor) override {
    constexpr int kWeaponTag = 0xF0;
    return Actors::FindLiveActorBoneTag(actor, kWeaponTag, 0);
  }
  void FadeTrails(std::uint8_t actor, int bone) override {
    Effects_FadeWeaponTrails(actor, bone);
  }
  void ToggleAshleyWeapon() override {
    LiveSetActorBattleMode(0, kToggleWeapon);
  }
};
}  // namespace

void ReleaseLiveQueuedActorAttacks() {
  LiveWorld world;
  ReleaseQueuedActorAttacks(world);
}
void WaitForLivePostActionMovement() {
  LiveWorld world;
  WaitForPostActionMovement(world);
}
void BeginLivePostActionRecovery(std::int32_t reward_mode) {
  LiveWorld world;
  BeginPostActionRecovery(reward_mode, world);
}
void BeginLiveActionTrailFade() {
  LiveSource source(State().queue[0]);
  LiveWorld world;
  BeginActionTrailFade(source, world);
}
void FinishLiveBattleAbilitySequence() {
  LiveWorld world;
  FinishBattleAbilitySequence(world);
}
void ResetLiveActionPhase() {
  LiveWorld world;
  ResetActionPhase(world);
}
void BeginLiveAshleyWeaponToggle() {
  LiveWorld world;
  BeginAshleyWeaponToggle(world);
}
}  // namespace VS::Battle
