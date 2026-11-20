// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPhasesLive.hpp"

#include <bit>
#include <cstdlib>
#include <iterator>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionPhases.hpp"
#include "Battle/Actor.hpp"
#include "Battle/BattleModeLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Effects/BoneTrailLive.hpp"
#include "Field/Interactions.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/Progress.hpp"
#include "State/Timers.hpp"

using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf("battle",
                     "Action phase refused: missing used state or actor");
    std::abort();
  }
}
ActionState& State() {
  Require(VS::Battle::actionState != nullptr);
  return *VS::Battle::actionState;
}
VS::Battle::Actor* ActorInSlot(int slot) {
  Require(slot >= 0 &&
          static_cast<std::size_t>(slot) < std::size(VS::Battle::actors));
  return VS::Battle::actors[slot];
}
struct LiveSource final : ActionPhaseSource {
  explicit LiveSource(ActionRequestRecord& action) : action_(action) {}
  std::uint8_t Actor() const override {
    return action_.primary_target.target.target_actor;
  }

 private:
  ActionRequestRecord& action_;
};
class LiveWorld final : public ActionPhaseWorld {
 public:
  std::optional<Actors::ActorCommand> Command(int slot) override {
    const auto* actor = ActorInSlot(slot);
    if (actor == nullptr) return std::nullopt;
    return Actors::ActorCommand{.state = actor->command_state,
                                .opcode = actor->command_opcode};
  }
  void SetCommandState(int slot, std::uint8_t state) override {
    auto* actor = ActorInSlot(slot);
    Require(actor != nullptr);
    actor->command_state = state;
  }
  std::uint8_t PrimaryActor() override {
    return State().selection.primary_target.target.target_actor;
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
    VS::Battle::recoveryRewardMode = mode;
  }
  void SetTickSpeed(std::uint32_t speed) override {
    VS::State::tickSpeed = std::bit_cast<std::int32_t>(speed);
  }
  void SetPhase(ActionPhase phase) override {
    VS::Battle::actionFlowMode = static_cast<std::uint32_t>(phase);
  }
  void SetControlMode(Field::FieldControlMode mode) override {
    VS::Field::controlMode = static_cast<std::uint32_t>(mode);
  }
  int TrailBone(std::uint8_t actor) override {
    constexpr int kWeaponTag = 0xF0;
    return Actors::FindLiveActorBoneTag(actor, kWeaponTag, 0);
  }
  void FadeTrails(std::uint8_t /*actor*/, int /*bone*/) override {
    // The original fade ignored its actor and bone; every trail fades.
    Effects::LiveFadeWeaponTrails();
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
