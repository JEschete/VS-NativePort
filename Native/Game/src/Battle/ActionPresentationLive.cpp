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
#include "Actors/SkeletonLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentation.hpp"
#include "Battle/ActionPreviewLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/ReactionDelay.hpp"
#include "Battle/TargetingLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "ActorAssets.h"
#include "Actors/Location.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Core/Diagnostics.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Input/ControllerLiveState.h"
#include "Items/Loadout.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/TextBox.h"
#include "State/Progress.h"
#include "State/Timers.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    Core_Errorf("battle",
                "Action presentation refused: missing used owner/index, "
                "invalid chain slot/category, or unsupported effect capacity "
                "(maximum 16 unique targets; duplicate input hits allowed)");
    std::abort();
  }
}
Battle_ActionState& State() {
  Require(Battle_actionState != nullptr);
  return *Battle_actionState;
}
Battle_Actor& Actor(int slot) {
  Require(slot >= 0 &&
          static_cast<std::size_t>(slot) < std::size(Battle_actors) &&
          Battle_actors[slot] != nullptr);
  return *Battle_actors[slot];
}
Battle_ActorStats& Stats(int slot) {
  auto* stats = Actor(slot).stats;
  Require(stats != nullptr);
  return *stats;
}
Battle_ActionDefinition& Definition(std::uint16_t action) {
  Require(action < std::size(Battle_actionDefinitions));
  return Battle_actionDefinitions[action];
}
Menu_Framework_ActionSelection& MenuSelection() {
  Require(Battle_actionSelection != nullptr);
  return *Battle_actionSelection;
}
struct LiveAction final : PresentationAction {
  explicit LiveAction(Battle_ActionRequest& value) : record(value) {}
  Battle_ActionRequest& record;
  std::uint16_t Id() const override { return record.actionId; }
  void ClearId() override { record.actionId = 0; }
  ActionHit Primary() const override {
    return std::bit_cast<ActionHit>(record.primaryTarget);
  }
  ActionHit Target(std::size_t index) const override {
    Require(index < std::size(record.targets));
    return std::bit_cast<ActionHit>(record.targets[index]);
  }
  std::uint16_t Count() const override { return record.targetCount; }
  std::uint16_t Animation() const override { return record.animationParameter; }
  std::uint8_t Variant() const override {
    return static_cast<std::uint8_t>(record.effectVariant);
  }
  ActionPoint Focus() const override {
    return std::bit_cast<ActionPoint>(record.focusPosition);
  }
};
Effects_ActionEffectTarget LiveTarget(const PresentationTarget& source) {
  Effects_ActionEffectTarget target{};
  target.type = source.kind;
  constexpr int kActor = 4;
  if (source.kind == kActor) {
    target.value.actor.actor = source.actor;
    target.value.actor.part = source.part;
  } else {
    target.value.position = std::bit_cast<SVECTOR>(source.position);
  }
  return target;
}
Effects_ActionEffectRequest LiveRequest(
    const ActionPresentationRequest& source) {
  Effects_ActionEffectRequest request{};
  request.effectId = source.effect;
  request.count = source.count;
  request.otherCount = source.other_count;
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
    return Bind(state.queue[state.queueIndex & kQueueMask]);
  }
  std::uint32_t QueueIndex() override { return State().queueIndex; }
  std::uint8_t SelectedPreview() override { return State().selectedPreview; }
  void BeginCommit() override {
    State().chainStep = 1;
    State().queueIndex = 0;
  }
  void SetZoom(std::uint8_t zoom) override { State().cameraZoomState = zoom; }
  void SetField(AbilityField field, int value) override;
  std::uint8_t ChainSlot() override {
    return static_cast<std::uint8_t>(State().chainAbilitySlot);
  }
  std::uint8_t MappedChain(std::size_t slot) override {
    Require(slot < std::size(Input_settings.mappedChainAbilities));
    return static_cast<std::uint8_t>(Input_settings.mappedChainAbilities[slot]);
  }
  bool HasPresentation(std::uint16_t action) override {
    return Definition(action).returnsToMenuOnCancel != 0;
  }
  std::uint8_t Effect(std::uint16_t action) override {
    return Definition(action).effectId;
  }
  std::uint8_t Zoom(std::uint16_t action) override {
    return static_cast<std::uint8_t>(Definition(action).opaqueDBit4);
  }
  std::uint32_t PlayerStatuses() override {
    Require(Battle_characterState != nullptr &&
            Battle_characterState->stats != nullptr);
    return static_cast<std::uint32_t>(Battle_characterState->stats->statuses);
  }
  void SetFlag(int flag) override { State::SetLiveScriptFlag(flag, 1); }
  void Animate(ActionPose pose, int actor, int parameter,
               std::uint32_t step) override;
  std::uint32_t WindowFrame(int actor) override {
    return static_cast<std::uint32_t>(
        Actors::LiveAnimationBattleWindowFrame(actor));
  }
  void SetTickSpeed(int speed) override { State_tickSpeed = speed; }
  void SetPhase(int phase) override {
    Battle_actionFlowMode = static_cast<std::uint32_t>(phase);
  }
  void StopSound(int sound) override {
    constexpr int kBattleBank = 0x7E;
    Audio::LiveStopSfx(kBattleBank, sound);
  }
  void ReleaseNotification() override { Menu_ReleaseSceneNotification(); }
  void ClearEffectMode() override { Effects_ClearActionEffectMode(); }
  void PrepareCamera() override {
    Field::LiveStorePreferredActionCameraDistance();
  }
  void ClearSphereTargets() override { Battle_ClearSphereTargets(); }
  void ClearPrimaryVolume() override { Battle_ClearPrimarySphereVolume(); }
  void ClearSecondaryVolume() override { Battle_ClearSecondarySphereVolume(); }
  void FreeSphere() override { Battle_FreeSphere(); }
  void ReleaseMenu() override { Menu::Framework::ReleaseLiveMenuRows(); }
  void ResetReaction(int actor) override {
    Ai::ResetActorAiReactionDelay(Ai::BattleActorReactionDelays(), actor);
  }
  void Recover() override { BeginLivePostActionRecovery(0); }
  void ReturnToMenu() override { LiveReturnToBattleActionMenu(); }
  std::int16_t SelectionKind() override {
    return MenuSelection().selectionKind;
  }
  std::uint16_t SelectedItem() override {
    return MenuSelection().executeAbility.halves[1];
  }
  void ConsumeItem(std::uint16_t item) override {
    Items::ConsumeLiveMiscItem(item);
  }
  void ClearSelectionKind() override { MenuSelection().selectionKind = 0; }
  void Disengage(int actor) override { Ai::DisengageLiveActorById(actor); }
  void CollectEnvironment() override { LiveCollectEnvironmentActionTargets(); }
  void CollectAll() override { LiveSetAllActionTargets(); }
  void CollectSingle() override { LiveSetSingleActionTarget(); }
  void ResolveSelection() override {
    auto& state = State();
    ResolveLiveActionTargets(state.selection.actionId, state.queue,
                             &state.selection, 1);
  }
  std::uint8_t Spawn(int actor) override { return Actor(actor).roomSpawnIndex; }
  std::uint8_t WeaponCategory() override {
    Require(Battle_characterState != nullptr);
    return Battle_characterState->equippedWeaponCategory;
  }
  std::uint16_t WeaponAttacks(std::size_t category) override {
    Require(category < std::size(Audio_Seq_unusedScoredata.weaponAttacks));
    return Audio_Seq_unusedScoredata.weaponAttacks[category];
  }
  void SetWeaponAttacks(std::size_t category, std::uint16_t count) override {
    Require(category < std::size(Audio_Seq_unusedScoredata.weaponAttacks));
    Audio_Seq_unusedScoredata.weaponAttacks[category] = count;
  }
  int TargetDistance() override { return Battle_ActionTargetDistance(); }
  void ClearSpellCommit() override { Battle_spellCommitFlag = 0; }
  std::uint16_t HealCount() override {
    return Audio_Seq_unusedScoredata.healCount;
  }
  void SetHealCount(std::uint16_t count) override {
    Audio_Seq_unusedScoredata.healCount = count;
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
  void HideMarkers() override { Battle_SetActorMarkersHidden(1); }
  void HideMinimap() override { Field::SetLiveMinimapVisible(0); }
  void ResumeRoomAnimations() override {
    Field::LiveSetRoomAnimationsPaused(0);
  }
  ActionPoint Placement(int actor) override {
    Actors_ActorLocationQuery placement{};
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
    return Stats(actor).defaultTargetLimb;
  }
  bool EligibleBystander(int actor) const override {
    const auto* value = Battle_actors[actor];
    return value != nullptr &&
           (value->allocationKind == 1 || value->allocationKind == 2);
  }
  void Dispatch(const ActionPresentationRequest& request, int mode,
                std::uint8_t variant) override {
    auto live = LiveRequest(request);
    Effects_BeginActionEffect(&live, mode, variant);
  }
  std::uint8_t SourceMaterial(int actor) override {
    return Actor(actor).equippedWeaponCategory;
  }
  std::uint8_t SourceModel(int actor) override;
  void QueueAsset(PresentationAsset asset) override {
    Content_ActorAssetRequest live{};
    constexpr int kEffectAsset = 7;
    live.assetKind = kEffectAsset;
    live.targetSlot = std::bit_cast<decltype(live.targetSlot)>(asset.slot);
    live.subKind = asset.kind;
    live.modelId = asset.model;
    live.material = std::bit_cast<decltype(live.material)>(asset.material);
    Content::QueueLiveActorAssetRequest(&live);
  }
  void HideHud() override { Battle_SetHudHidden(1); }
  bool InformationEnabled() override { return Input_settings.information != 0; }
  void ShowAction(std::uint16_t action) override {
    constexpr int kActionNotification = 0xB;
    Menu_ShowSceneNotification(kActionNotification, action, 1);
  }

 private:
  PresentationAction& Bind(Battle_ActionRequest& value) {
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
      state.previousChainButtons = byte;
      break;
    case AbilityField::kPreviousChainInput:
      state.previousChainInput = byte;
      break;
    case AbilityField::kChainAbilitySlot:
      state.chainAbilitySlot = byte;
      break;
    case AbilityField::kAbilityInputSucceeded:
      state.abilityInputSucceeded = byte;
      break;
    case AbilityField::kAbilityInputAttempted:
      state.abilityInputAttempted = byte;
      break;
    case AbilityField::kChainContinued:
      state.chainContinued = byte;
      break;
    case AbilityField::kAbilityTimer:
      state.abilityTimer = static_cast<std::uint16_t>(value);
      break;
    case AbilityField::kAbilityWindow:
      state.abilityWindow = static_cast<std::uint16_t>(value);
      break;
  }
}
void LiveWorld::Animate(ActionPose pose, int actor, int parameter,
                        std::uint32_t step) {
  auto* focus = &State().selection.focusPosition;
  switch (pose) {
    case ActionPose::kMelee:
      Battle_PlayMeleeAttackAnimation(actor, focus, parameter);
      break;
    case ActionPose::kSpell:
      Battle_PlaySpellCastAnimation(actor, focus, parameter);
      break;
    case ActionPose::kBreakArt:
      Battle_PlayBreakArtAnimation(actor, focus, parameter);
      break;
    case ActionPose::kSpecial:
      Battle_PlaySpecialActionAnimation(actor, focus, parameter);
      break;
    case ActionPose::kChain:
      Battle_PlayChainAbilityAnimation(actor, focus, parameter,
                                       std::bit_cast<int>(step));
      break;
    case ActionPose::kDefense:
      Battle_PlayDefenseAbilityAnimation(actor, focus, parameter);
      break;
  }
}
std::uint8_t LiveWorld::SourceModel(int slot) {
  const auto& actor = Actor(slot);
  Require(actor.stats != nullptr);
  const auto limb = static_cast<std::uint16_t>(actor.commandArg0);
  constexpr int kHalfwordBits = 16;
  const auto choice =
      static_cast<std::uint32_t>(actor.commandArg0) >> kHalfwordBits;
  Require(limb < std::size(actor.stats->limbActions) &&
          choice < std::size(actor.stats->limbActions[limb]));
  // Imported char bitfields are signed; the original loaded the high nibble
  // with LBU/SRL, not a sign-extended four-bit value.
  constexpr std::uint8_t kModelNibble = 0x0F;
  return static_cast<std::uint8_t>(
             actor.stats->limbActions[limb][choice].effectModelId) &
         kModelNibble;
}
}  // namespace
// decomp: func_80070278 (src/BATTLE/BATTLE.PRG/146C.c:3510).
void StartLiveActionAnimation() {
  LiveWorld world;
  Require(StartActionAnimation(world));
}
// decomp: func_800704B0 (src/BATTLE/BATTLE.PRG/146C.c:3572).
void AdvanceLiveActionEffectPhase() {
  LiveWorld world;
  AdvanceActionEffectPhase(world);
}
// decomp: func_80070F28 (src/BATTLE/BATTLE.PRG/146C.c:3775).
void CommitLiveSelectedAction(int target_selection_mode) {
  LiveWorld world;
  Require(CommitSelectedAction(target_selection_mode, world));
}
// decomp: func_8007138C (src/BATTLE/BATTLE.PRG/146C.c:3906).
void StartLiveActionPresentation() {
  LiveWorld world;
  Require(StartActionPresentation(world));
}
void BuildLiveActionPresentationRequest(Battle_ActionRequest* action,
                                        Effects_ActionEffectRequest* request) {
  Require(action != nullptr && request != nullptr);
  LiveAction owner(*action);
  LiveWorld world;
  ActionPresentationRequest result{};
  result.effect = world.Effect(owner.Id());
  Require(BuildActionPresentationRequest(owner, world, result));
  *request = LiveRequest(result);
}
}  // namespace VS::Battle
