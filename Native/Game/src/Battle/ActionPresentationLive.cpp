// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPresentationLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <vector>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionAnimation.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionCameraSelectionLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentation.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/ReactionDelay.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/SphereLifecycleLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/Runtime.hpp"
#include "Field/CameraLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBox.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/KillScoreLive.hpp"
#include "State/Progress.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::SVECTOR;

using VS::Battle::ActionDefinition;
using VS::Battle::ActionRequestRecord;
using VS::Battle::ActionState;

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf(
        "battle",
        "Action presentation refused: missing used owner/index, "
        "invalid chain slot/category, or unsupported effect capacity "
        "(maximum 16 unique targets; duplicate input hits allowed)");
    std::abort();
  }
}
ActionState& State() {
  Require(VS::Battle::actionState != nullptr);
  return *VS::Battle::actionState;
}
VS::Battle::Actor& ActorInSlot(int slot) {
  Require(slot >= 0 &&
          static_cast<std::size_t>(slot) < std::size(VS::Battle::actors) &&
          VS::Battle::actors[slot] != nullptr);
  return *VS::Battle::actors[slot];
}
VS::Battle::ActorStats& Stats(int slot) {
  auto* stats = ActorInSlot(slot).stats;
  Require(stats != nullptr);
  return *stats;
}
ActionDefinition& Definition(std::uint16_t action) {
  Require(action < std::size(VS::Battle::actionDefinitions));
  return VS::Battle::actionDefinitions[action];
}
Menu::Framework::ActionSelection& MenuSelection() {
  Require(VS::Battle::actionSelection != nullptr);
  return *VS::Battle::actionSelection;
}
struct LiveAction final : PresentationAction {
  explicit LiveAction(ActionRequestRecord& value) : record(value) {}
  ActionRequestRecord& record;
  std::uint16_t Id() const override { return record.action_id; }
  void ClearId() override { record.action_id = 0; }
  ActionHit Primary() const override {
    return std::bit_cast<ActionHit>(record.primary_target);
  }
  ActionHit Target(std::size_t index) const override {
    Require(index < std::size(record.targets));
    return std::bit_cast<ActionHit>(record.targets[index]);
  }
  std::uint16_t Count() const override { return record.target_count; }
  std::uint16_t Animation() const override { return record.target_redirected; }
  std::uint8_t Variant() const override {
    return static_cast<std::uint8_t>(record.affinity);
  }
  ActionPoint Focus() const override {
    return std::bit_cast<ActionPoint>(record.focus_position);
  }
};
VS::Effects::LiveActionEffectTarget LiveTarget(
    const PresentationTarget& source) {
  VS::Effects::LiveActionEffectTarget target{};
  target.type = source.kind;
  constexpr int kActor = 4;
  if (source.kind == kActor) {
    target.SetActor({.actor = source.actor,
                     .event_actor_slot = 0,
                     .part = source.part,
                     .reserved3 = 0});
  } else {
    target.position = std::bit_cast<SVECTOR>(source.position);
  }
  return target;
}
VS::Effects::LiveActionEffectRequest LiveRequest(
    const ActionPresentationRequest& source) {
  VS::Effects::LiveActionEffectRequest request{};
  request.effect_id = source.effect;
  request.count = source.count;
  request.other_count = source.other_count;
  request.source = LiveTarget(source.source);
  for (std::size_t i = 0; i < source.count; ++i) {
    request.targets[i] = LiveTarget(source.targets.at(i));
  }
  for (std::size_t i = 0; i < source.other_count; ++i) {
    request.others[i] = source.others.at(i);
  }
  request.position = std::bit_cast<SVECTOR>(source.focus);
  return request;
}
class LiveWorld final : public ActionPresentationWorld {
 public:
  PresentationAction& Selection() override { return Bind(State().selection); }
  PresentationAction& Queued() override {
    auto& state = State();
    constexpr std::uint32_t kQueueMask = 3;
    return Bind(state.queue[state.queue_index & kQueueMask]);
  }
  std::uint32_t QueueIndex() override { return State().queue_index; }
  std::uint8_t SelectedPreview() override { return State().selected_preview; }
  void BeginCommit() override {
    State().chain_step = 1;
    State().queue_index = 0;
  }
  void SetZoom(std::uint8_t zoom) override { State().camera_zoom_state = zoom; }
  void SetField(AbilityField field, int value) override;
  std::uint8_t ChainSlot() override {
    return static_cast<std::uint8_t>(State().chain_ability_slot);
  }
  std::uint8_t MappedChain(std::size_t slot) override {
    Require(slot < std::size(VS::Input::settings.mapped_chain_abilities));
    return static_cast<std::uint8_t>(
        VS::Input::settings.mapped_chain_abilities[slot]);
  }
  bool HasPresentation(std::uint16_t action) override {
    return Definition(action).returns_to_menu_on_cancel != 0;
  }
  std::uint8_t Effect(std::uint16_t action) override {
    return Definition(action).effect_id;
  }
  std::uint8_t Zoom(std::uint16_t action) override {
    return static_cast<std::uint8_t>(Definition(action).camera_zoom_mode);
  }
  std::uint32_t PlayerStatuses() override {
    Require(VS::Battle::characterState != nullptr &&
            VS::Battle::characterState->stats != nullptr);
    return static_cast<std::uint32_t>(
        VS::Battle::characterState->stats->statuses);
  }
  void SetFlag(int flag) override { State::SetLiveScriptFlag(flag, 1); }
  void Animate(ActionPose pose, int actor, int parameter,
               std::uint32_t step) override;
  std::uint32_t WindowFrame(int actor) override {
    return static_cast<std::uint32_t>(
        Actors::LiveAnimationBattleWindowFrame(actor));
  }
  void SetTickSpeed(int speed) override { VS::State::tickSpeed = speed; }
  void SetPhase(int phase) override {
    VS::Battle::actionFlowMode = static_cast<std::uint32_t>(phase);
  }
  void StopSound(int sound) override {
    constexpr int kBattleBank = 0x7E;
    Audio::LiveStopSfx(kBattleBank, sound);
  }
  void ReleaseNotification() override { Menu::ReleaseLiveSceneNotification(); }
  void ClearEffectMode() override { Effects::LiveClearActionEffectMode(); }
  void PrepareCamera() override {
    Field::LiveStorePreferredActionCameraDistance();
  }
  void ClearSphereTargets() override { ClearSphereTargetsEntry(); }
  void ClearPrimaryVolume() override { ClearPrimarySphereVolumeEntry(); }
  void ClearSecondaryVolume() override { ClearSecondarySphereVolumeEntry(); }
  void FreeSphere() override { FreeSphereEntry(); }
  void ReleaseMenu() override { Menu::Framework::ReleaseLiveMenuRows(); }
  void ResetReaction(int actor) override {
    Ai::ResetActorAiReactionDelay(Ai::BattleActorReactionDelays(), actor);
  }
  void Recover() override { BeginLivePostActionRecovery(0); }
  void ReturnToMenu() override { LiveReturnToBattleActionMenu(); }
  std::int16_t SelectionKind() override {
    return MenuSelection().selection_kind;
  }
  std::uint16_t SelectedItem() override {
    return Menu::Framework::ExecuteAbilityItem(MenuSelection());
  }
  void ConsumeItem(std::uint16_t item) override {
    Items::ConsumeLiveMiscItem(item);
  }
  void ClearSelectionKind() override { MenuSelection().selection_kind = 0; }
  void Disengage(int actor) override { Ai::DisengageLiveActorById(actor); }
  void CollectEnvironment() override { LiveCollectEnvironmentActionTargets(); }
  void CollectAll() override { LiveSetAllActionTargets(); }
  void CollectSingle() override { LiveSetSingleActionTarget(); }
  void ResolveSelection() override {
    auto& state = State();
    ResolveLiveActionTargets(state.selection.action_id, state.queue.data(),
                             &state.selection, 1);
  }
  std::uint8_t Spawn(int actor) override {
    return ActorInSlot(actor).room_spawn_index;
  }
  std::uint8_t WeaponCategory() override {
    Require(VS::Battle::characterState != nullptr);
    return VS::Battle::characterState->equipped_weapon_category;
  }
  std::uint16_t WeaponAttacks(std::size_t category) override {
    Require(category < std::size(VS::State::scoreData.weapon_attacks));
    return VS::State::scoreData.weapon_attacks[category];
  }
  void SetWeaponAttacks(std::size_t category, std::uint16_t count) override {
    Require(category < std::size(VS::State::scoreData.weapon_attacks));
    VS::State::scoreData.weapon_attacks[category] = count;
  }
  int TargetDistance() override { return LiveActionTargetDistance(); }
  void ResetWeaponKillStreak() override { State::ResetLiveWeaponKillStreak(); }
  std::uint16_t HealCount() override { return VS::State::scoreData.heal_count; }
  void SetHealCount(std::uint16_t count) override {
    VS::State::scoreData.heal_count = count;
  }
  void RestoreIdle(int actor) override {
    Actors::RestoreLiveActorIdlePose(actor);
  }
  void EnableClut() override { Render::SetLiveClutAnimationEnabled(1); }
  void TransformColors() override {
    constexpr int kColorMode = 8;
    constexpr int kColorSpeed = 4;
    Render::TransformLiveSceneColors(kColorMode, kColorSpeed, 0, 0, 0);
  }
  void FreezeAnimations() override { Actors::FreezeLiveActorAnimations(); }
  void HideMarkers() override { VS::Battle::SetLiveActorMarkersHidden(1); }
  void HideMinimap() override { Field::SetLiveMinimapVisible(0); }
  void ResumeRoomAnimations() override {
    Field::LiveSetRoomAnimationsPaused(0);
  }
  ActionPoint Placement(int actor) override {
    VS::Actors::ActorLocationQuery placement{};
    Require(Actors::LiveActorPlacement(actor, &placement) >= 0);
    return {placement.location.position.vx, placement.location.position.vy,
            placement.location.position.vz, 0};
  }
  void PlaySound(int sound, ActionPoint position) override {
    constexpr int kBattleBank = 0x7E;
    auto live = std::bit_cast<SVECTOR>(position);
    Audio::LivePlayWorldSfx(kBattleBank, sound, &live);
  }
  int BoneTag(int actor, int limb) const override {
    return Actors::FindLiveActorBoneTag(actor, limb, 2);
  }
  std::uint8_t DefaultLimb(int actor) const override {
    return Stats(actor).default_target_limb;
  }
  bool EligibleBystander(int actor) const override {
    const auto* value = VS::Battle::actors[actor];
    return value != nullptr &&
           (value->allocation_kind == 1 || value->allocation_kind == 2);
  }
  void Dispatch(const ActionPresentationRequest& request, int mode,
                std::uint8_t variant) override {
    auto live = LiveRequest(request);
    VS::Effects::LiveBeginActionEffect(&live, mode, variant);
  }
  std::uint8_t SourceMaterial(int actor) override {
    return ActorInSlot(actor).equipped_weapon_category;
  }
  std::uint8_t SourceModel(int actor) override;
  void QueueAsset(PresentationAsset asset) override {
    VS::Content::LegacyActorAssetRequest live{};
    constexpr int kEffectAsset = 7;
    live.asset_kind = kEffectAsset;
    live.target_slot = std::bit_cast<decltype(live.target_slot)>(asset.slot);
    live.sub_kind = asset.kind;
    live.model_id = asset.model;
    live.material = std::bit_cast<decltype(live.material)>(asset.material);
    Content::QueueLiveActorAssetRequest(&live);
  }
  void HideHud() override { VS::Battle::SetLiveHudHidden(1); }
  bool InformationEnabled() override {
    return VS::Input::settings.information != 0;
  }
  void ShowAction(std::uint16_t action) override {
    constexpr int kActionNotification = 0xB;
    Menu::ShowLiveSceneNotification(kActionNotification, action, 1);
  }

 private:
  PresentationAction& Bind(ActionRequestRecord& value) {
    for (auto& action : actions_) {
      if (&action->record == &value) return *action;
    }
    return *actions_.emplace_back(std::make_unique<LiveAction>(value));
  }
  std::vector<std::unique_ptr<LiveAction>> actions_;
};
void LiveWorld::SetField(AbilityField field, int value) {
  auto& state = State();
  const auto byte = static_cast<char>(value);
  switch (field) {
    case AbilityField::kPreviousChainButtons:
      state.previous_chain_buttons = byte;
      break;
    case AbilityField::kPreviousChainInput:
      state.previous_chain_input = byte;
      break;
    case AbilityField::kChainAbilitySlot:
      state.chain_ability_slot = byte;
      break;
    case AbilityField::kAbilityInputSucceeded:
      state.ability_input_succeeded = byte;
      break;
    case AbilityField::kAbilityInputAttempted:
      state.ability_input_attempted = byte;
      break;
    case AbilityField::kDefenseContinued:
      state.defense_continued = byte;
      break;
    case AbilityField::kAbilityTimer:
      state.ability_timer = static_cast<std::uint16_t>(value);
      break;
    case AbilityField::kAbilityWindow:
      state.ability_window = static_cast<std::uint16_t>(value);
      break;
  }
}
void LiveWorld::Animate(ActionPose pose, int actor, int parameter,
                        std::uint32_t step) {
  auto* focus = &State().selection.focus_position;
  switch (pose) {
    case ActionPose::kMelee:
      PlayLiveMeleeAttackAnimation(actor, focus, parameter);
      break;
    case ActionPose::kSpell:
      PlayLiveSpellCastAnimation(actor, focus, parameter);
      break;
    case ActionPose::kBreakArt:
      PlayLiveBreakArtAnimation(actor, focus, parameter);
      break;
    case ActionPose::kSpecial:
      PlayLiveSpecialActionAnimation(actor, focus, parameter);
      break;
    case ActionPose::kChain:
      PlayLiveChainAbilityAnimation(actor, focus, parameter,
                                    std::bit_cast<int>(step));
      break;
    case ActionPose::kDefense:
      PlayLiveDefenseAbilityAnimation(actor, focus, parameter);
      break;
  }
}
std::uint8_t LiveWorld::SourceModel(int slot) {
  const auto& actor = ActorInSlot(slot);
  Require(actor.stats != nullptr);
  const auto limb = static_cast<std::uint16_t>(actor.command_arg0);
  constexpr int kHalfwordBits = 16;
  const auto choice =
      static_cast<std::uint32_t>(actor.command_arg0) >> kHalfwordBits;
  Require(limb < std::size(actor.stats->limb_actions) &&
          choice < std::size(actor.stats->limb_actions[limb]));
  // Imported char bitfields are signed; the original loaded the high nibble
  // with LBU/SRL, not a sign-extended four-bit value.
  constexpr std::uint8_t kModelNibble = 0x0F;
  return static_cast<std::uint8_t>(
             actor.stats->limb_actions[limb][choice].effect_model_id) &
         kModelNibble;
}
}  // namespace
// decomp: func_80070278 (src/BATTLE/BATTLE.PRG/146C.c:3219).
void StartLiveActionAnimation() {
  LiveWorld world;
  Require(StartActionAnimation(world));
}
// decomp: func_800704B0 (src/BATTLE/BATTLE.PRG/146C.c:3282).
void AdvanceLiveActionEffectPhase() {
  LiveWorld world;
  AdvanceActionEffectPhase(world);
}
// decomp: func_80070F28 (src/BATTLE/BATTLE.PRG/146C.c:3487).
void CommitLiveSelectedAction(int target_selection_mode) {
  LiveWorld world;
  Require(CommitSelectedAction(target_selection_mode, world));
}
// decomp: func_8007138C (src/BATTLE/BATTLE.PRG/146C.c:3618).
void StartLiveActionPresentation() {
  LiveWorld world;
  Require(StartActionPresentation(world));
}
void BuildLiveActionPresentationRequest(
    ActionRequestRecord* action,
    VS::Effects::LiveActionEffectRequest* request) {
  Require(action != nullptr && request != nullptr);
  LiveAction owner(*action);
  LiveWorld world;
  ActionPresentationRequest result{};
  result.effect = world.Effect(owner.Id());
  Require(BuildActionPresentationRequest(owner, world, result));
  *request = LiveRequest(result);
}
}  // namespace VS::Battle
