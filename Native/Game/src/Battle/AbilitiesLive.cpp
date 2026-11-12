// Hand-maintained (tools/port/owned.csv).
// Binds the typed battle abilities (Abilities.cpp) to the live action state,
// battle actors, kill and score progress, ability mapping and presentation.
#include "Battle/AbilitiesLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Log.hpp"
#include "Field/GameModeLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "State/ScriptFlagsLive.hpp"

extern "C" {
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Perception.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/Gimmicks.h"
#include "Field/PlatformPlacementLive.h"
#include "Input/ControllerLiveState.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Menu/TextBox.h"
#include "State/Progress.h"
#include "State/Timers.h"
}

namespace {
namespace battle = VS::Battle;
using battle::AbilityField;
using battle::AbilityKind;

// The pose the player plays while learning an ability.
constexpr int kAbilityLearnedPose = 6;
// Menu_ShowSceneNotification kind of the "learned" notice.
constexpr int kActionLearnedNotification = 0xC;
// The action availability word keeps its flags in the top byte.
constexpr std::uint32_t kActionAvailableMask = 0xFF000000u;
constexpr std::uint32_t kEffectBusyMask = 3;
// The pending unlocks pack the break art above the battle ability.
constexpr int kPendingBreakArtShift = 16;
// Action queue slots (queue[queueIndex & 3]).
constexpr std::uint32_t kQueueSlotMask = 3;

template <typename Array>
bool InRange(const Array& array, int index) {
  return index >= 0 && static_cast<std::size_t>(index) < std::size(array);
}

Battle_Actor* LiveActor(int actor) {
  return InRange(Battle_actors, actor) ? Battle_actors[actor] : nullptr;
}

const Battle_ActorStats* LiveStats(int actor) {
  const Battle_Actor* record = LiveActor(actor);
  return record != nullptr ? record->stats : nullptr;
}

battle::ActionRequestView View(const Battle_ActionRequest& request) {
  battle::ActionRequestView view;
  view.action_id = request.actionId;
  view.primary_actor = request.primaryTarget.target.targetActor;
  view.primary_targets_position = request.primaryTarget.targetsPosition != 0;
  view.target_count = request.targetCount;
  view.first_target_actor = request.targets[0].target.targetActor;
  view.first_target_targets_position = request.targets[0].targetsPosition != 0;
  return view;
}

class LiveAbilityHost final : public battle::AbilityHost {
 public:
  // The action state may be absent for the unlock queries, which never read
  // it; the callers that do read it refuse an absent one first.
  explicit LiveAbilityHost(Battle_ActionState* state) : state_(state) {}

  [[nodiscard]] std::uint32_t QueueIndex() const override {
    return state_->queueIndex;
  }
  void SetQueueIndex(std::uint32_t index) override {
    state_->queueIndex = index;
  }
  [[nodiscard]] std::uint32_t ChainStep() const override {
    return state_->chainStep;
  }
  void SetChainStep(std::uint32_t step) override { state_->chainStep = step; }
  [[nodiscard]] int Field(AbilityField field) const override;
  void SetField(AbilityField field, int value) override;
  [[nodiscard]] battle::ActionRequestView QueuedAction() const override {
    return View(Queued());
  }
  [[nodiscard]] battle::ActionRequestView Selection() const override {
    return View(state_->selection);
  }
  [[nodiscard]] int TickSpeed() const override { return State_tickSpeed; }
  void SetTickSpeed(int speed) override { State_tickSpeed = speed; }
  void SetActionFlowMode(int mode) override {
    Battle_actionFlowMode = static_cast<u_int>(mode);
  }
  [[nodiscard]] int RecoveryRewardMode() const override {
    return Battle_recoveryRewardMode;
  }

  [[nodiscard]] battle::SelectionTargetView SelectionTarget(
      int index) const override;
  void CopySelectionTargetToPrimary(int index) override;
  void SetSelectionPrimaryVitals(int hp, int mp) override {
    state_->selection.primaryTarget.target.hp = static_cast<std::int16_t>(hp);
    state_->selection.primaryTarget.target.mp = static_cast<std::int16_t>(mp);
  }
  void SavePrimaryTarget() override {
    saved_primary_ = state_->selection.primaryTarget;
  }
  void RestoreSavedTargetAsFirst(int limb) override {
    state_->selection.targets[0] = saved_primary_;
    state_->selection.targets[0].target.targetLimb =
        static_cast<signed char>(limb);
  }
  void SetSelectionTargetCount(int count) override {
    state_->selection.targetCount = static_cast<u_short>(count);
  }
  void FocusSelectionOnActor(int actor) override;

  [[nodiscard]] int MappedAbility(AbilityKind kind, int index) const override;
  [[nodiscard]] bool ActionAvailable(int action_id) const override;
  [[nodiscard]] bool ActionDefinitionUnlocked(int action_id) const override;
  void SetActionDefinitionUnlocked(int action_id) override;
  [[nodiscard]] int ActorRoomSpawnIndex(int actor) const override;
  [[nodiscard]] int ActorHp(int actor) const override;
  [[nodiscard]] int ActorDefaultTargetLimb(int actor) const override;
  [[nodiscard]] int SpellClassCount(int spell_class) const override;
  [[nodiscard]] int SpellClassSpellId(int spell_class,
                                      int index) const override;
  [[nodiscard]] int AbilityActionId(AbilityKind kind, int index) const override;

  [[nodiscard]] int EquippedWeaponCategory() const override;
  [[nodiscard]] int ArtsLearned(int category) const override;
  [[nodiscard]] int WeaponCategoryKills(int category) const override;
  [[nodiscard]] int BreakArtRequirement(int category, int tier) const override;
  [[nodiscard]] int BattleAbilitiesUnlocked() const override {
    return Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked;
  }
  [[nodiscard]] int BattleAbilityRequirement(int index) const override;
  [[nodiscard]] int TotalKills() const override {
    return Audio_Seq_unusedArtsStatus.kills.totalKills;
  }
  void SetTotalKills(int kills) override {
    Audio_Seq_unusedArtsStatus.kills.totalKills = static_cast<u_short>(kills);
  }
  [[nodiscard]] int MaxChain() const override {
    return Audio_Seq_unusedScoredata.maxChain;
  }
  void SetMaxChain(int chain) override {
    Audio_Seq_unusedScoredata.maxChain = static_cast<u_short>(chain);
  }
  [[nodiscard]] int ChainStreak() const override {
    return Audio_Seq_unusedScoredata.chainStreak;
  }
  void SetChainStreak(int streak) override {
    Audio_Seq_unusedScoredata.chainStreak = static_cast<u_short>(streak);
  }
  [[nodiscard]] int ScriptFlag(int flag) const override {
    return VS::State::LiveScriptFlag(flag);
  }

  void WaitForPostActionMovement() override {
    Battle_WaitForPostActionMovement();
  }
  void FinishBattleAbilitySequence() override {
    Battle_FinishBattleAbilitySequence();
  }
  void PlayAbilityLearnedPose() override {
    VS::Actors::PlayLiveActorActionPose(0, nullptr, kAbilityLearnedPose);
  }
  void SpawnAbilityLearnedEffect() override {
    Effects_SpawnAbilityLearnedEffect();
  }
  void PauseMovingPlatforms() override {
    Field_SetMovingPlatformsPausedEntry(1);
  }
  void PauseFieldForMenu() override { VS::Field::LivePauseFieldForMenu(); }
  void ShowActionLearnedNotification(int action_id) override {
    Menu_ShowSceneNotification(kActionLearnedNotification, action_id, 1);
  }
  void OpenAbilityUnlockMenu(battle::PendingAbilityUnlocks unlocks) override {
    VS::Menu::Framework::OpenLiveAbilityUnlockMenu(
        unlocks.break_art ? 1 : 0, unlocks.battle_ability ? 1 : 0);
  }
  void ReleaseActorSequence(int actor, int kind) override {
    VS::Content::ReleaseLiveActorSequence(actor, kind);
  }
  void NotifyQueuedActionRelationships() override {
    VS::Battle::Ai::NotifyLiveActionRelationshipChanges(&Queued());
  }
  void ResolveSelectionIntoQueue(int action_id) override {
    Battle_ResolveActionTargets(action_id, &Queued(), &state_->selection, 1);
  }
  void QueueChainCount(int count) override { Battle_QueueChainCount(count); }
  void ShowAbilityTimingCue(int frames) override {
    Battle_ShowAbilityTimingCue(frames);
  }
  void QueueAbilityTimingResult(int result) override {
    Battle_QueueAbilityTimingResult(result);
  }
  void ClearAbilityInputResult() override { Battle_abilityInputResult = 0; }
  [[nodiscard]] std::uint32_t PressedButtons() const override {
    return Input_buttonsPressed.allButtons;
  }
  [[nodiscard]] bool EffectsBusy() const override {
    return (static_cast<std::uint32_t>(Effects_effectBusyFlags) &
            kEffectBusyMask) != 0;
  }
  [[nodiscard]] bool IsActorActionAnimationBusy(int actor) const override {
    return VS::Actors::IsLiveActorActionAnimationBusy(actor) != 0;
  }
  void RestoreActorIdlePose(int actor) override {
    VS::Actors::RestoreLiveActorIdlePose(actor);
  }

 private:
  [[nodiscard]] Battle_ActionRequest& Queued() const {
    return state_->queue[state_->queueIndex & kQueueSlotMask];
  }

  Battle_ActionState* state_;
  Battle_HitResult saved_primary_{};
};

int LiveAbilityHost::Field(AbilityField field) const {
  switch (field) {
    case AbilityField::kPreviousChainButtons:
      return state_->previousChainButtons;
    case AbilityField::kChainAbilitySlot:
      return state_->chainAbilitySlot;
    case AbilityField::kPreviousChainInput:
      return state_->previousChainInput;
    case AbilityField::kAbilityInputSucceeded:
      return state_->abilityInputSucceeded;
    case AbilityField::kAbilityInputAttempted:
      return state_->abilityInputAttempted;
    case AbilityField::kChainContinued:
      return state_->chainContinued;
    case AbilityField::kAbilityTimer:
      return state_->abilityTimer;
    case AbilityField::kAbilityWindow:
      return state_->abilityWindow;
  }
  return 0;
}

void LiveAbilityHost::SetField(AbilityField field, int value) {
  const auto byte = static_cast<char>(value);
  switch (field) {
    case AbilityField::kPreviousChainButtons:
      state_->previousChainButtons = byte;
      break;
    case AbilityField::kChainAbilitySlot:
      state_->chainAbilitySlot = byte;
      break;
    case AbilityField::kPreviousChainInput:
      state_->previousChainInput = byte;
      break;
    case AbilityField::kAbilityInputSucceeded:
      state_->abilityInputSucceeded = byte;
      break;
    case AbilityField::kAbilityInputAttempted:
      state_->abilityInputAttempted = byte;
      break;
    case AbilityField::kChainContinued:
      state_->chainContinued = byte;
      break;
    case AbilityField::kAbilityTimer:
      state_->abilityTimer = static_cast<u_short>(value);
      break;
    case AbilityField::kAbilityWindow:
      state_->abilityWindow = static_cast<u_short>(value);
      break;
  }
}

battle::SelectionTargetView LiveAbilityHost::SelectionTarget(int index) const {
  battle::SelectionTargetView view;
  if (!InRange(state_->selection.targets, index)) {
    VS_LOG_WARN("battle.ability", "selection target index={} out of range",
                index);
    return view;
  }
  const Battle_HitResult& target = state_->selection.targets[index];
  view.actor = target.target.targetActor;
  view.hp = target.target.hp;
  view.mp = target.target.mp;
  view.has_vital_effect = target.hpEffect != 0 || target.mpEffect != 0;
  return view;
}

void LiveAbilityHost::CopySelectionTargetToPrimary(int index) {
  if (!InRange(state_->selection.targets, index)) {
    VS_LOG_WARN("battle.ability", "selection target index={} out of range",
                index);
    return;
  }
  state_->selection.primaryTarget = state_->selection.targets[index];
}

void LiveAbilityHost::FocusSelectionOnActor(int actor) {
  Actors_ActorLocationQuery position{};
  VS::Actors::LiveActorPlacement(actor, &position);
  state_->selection.focusPosition.vx = position.location.position.vx;
  state_->selection.focusPosition.vz = position.location.position.vz;
  state_->selection.focusPosition.vy = position.location.position.vy;
}

int LiveAbilityHost::MappedAbility(AbilityKind kind, int index) const {
  const auto& mapped = kind == AbilityKind::kChain
                           ? Input_settings.mappedChainAbilities
                           : Input_settings.mappedDefenseAbilities;
  if (!InRange(mapped, index)) {
    VS_LOG_WARN("battle.ability", "mapped ability index={} out of range",
                index);
    return 0;
  }
  // The settings hold unsigned action ids.
  return static_cast<std::uint8_t>(mapped[index]);
}

bool LiveAbilityHost::ActionAvailable(int action_id) const {
  if (Battle_characterState == nullptr ||
      Battle_characterState->stats == nullptr) {
    VS_LOG_WARN("battle.ability", "action {} cost without character stats",
                action_id);
    return false;
  }
  const auto availability =
      static_cast<std::uint32_t>(battle::LiveActionCostAndAvailability(
          static_cast<u_int>(action_id), Battle_characterState->stats, 0));
  return (availability & kActionAvailableMask) != 0;
}

bool LiveAbilityHost::ActionDefinitionUnlocked(int action_id) const {
  if (!InRange(Battle_actionDefinitions, action_id)) {
    VS_LOG_WARN("battle.ability", "action definition {} out of range",
                action_id);
    return false;
  }
  return Battle_actionDefinitions[action_id].unlocked != 0;
}

void LiveAbilityHost::SetActionDefinitionUnlocked(int action_id) {
  if (!InRange(Battle_actionDefinitions, action_id)) {
    VS_LOG_WARN("battle.ability", "unlock of action definition {} out of range",
                action_id);
    return;
  }
  Battle_actionDefinitions[action_id].unlocked = 1;
}

int LiveAbilityHost::ActorRoomSpawnIndex(int actor) const {
  const Battle_Actor* record = LiveActor(actor);
  if (record == nullptr) {
    VS_LOG_WARN("battle.ability", "spawn slot of absent actor {}", actor);
    return 0;
  }
  return record->roomSpawnIndex;
}

int LiveAbilityHost::ActorHp(int actor) const {
  const Battle_ActorStats* stats = LiveStats(actor);
  if (stats == nullptr) {
    VS_LOG_WARN("battle.ability", "hp of absent actor {}", actor);
    return 0;
  }
  return stats->currentHp;
}

int LiveAbilityHost::ActorDefaultTargetLimb(int actor) const {
  const Battle_ActorStats* stats = LiveStats(actor);
  if (stats == nullptr) {
    VS_LOG_WARN("battle.ability", "target limb of absent actor {}", actor);
    return 0;
  }
  return static_cast<int>(stats->defaultTargetLimb);
}

int LiveAbilityHost::SpellClassCount(int spell_class) const {
  if (!InRange(Battle_spellClassCounts, spell_class)) {
    VS_LOG_WARN("battle.ability", "spell class {} out of range", spell_class);
    return 0;
  }
  return Battle_spellClassCounts[spell_class];
}

int LiveAbilityHost::SpellClassSpellId(int spell_class, int index) const {
  if (!InRange(Battle_spellClassSpellIds, spell_class) ||
      Battle_spellClassSpellIds[spell_class] == nullptr || index < 0) {
    VS_LOG_WARN("battle.ability", "spell class {} entry {} unavailable",
                spell_class, index);
    return -1;
  }
  return Battle_spellClassSpellIds[spell_class][index];
}

int LiveAbilityHost::AbilityActionId(AbilityKind kind, int index) const {
  const auto& ids = kind == AbilityKind::kChain
                        ? Battle_chainAbilityActionIds
                        : Battle_defenseAbilityActionIds;
  if (!InRange(ids, index)) {
    VS_LOG_WARN("battle.ability", "ability action index={} out of range",
                index);
    return -1;
  }
  // The tables hold unsigned action ids.
  return static_cast<std::uint8_t>(ids[index]);
}

int LiveAbilityHost::EquippedWeaponCategory() const {
  if (Battle_characterState == nullptr) {
    VS_LOG_WARN("battle.ability", "weapon category without character state");
    return 0;
  }
  return Battle_characterState->equippedWeaponCategory;
}

int LiveAbilityHost::ArtsLearned(int category) const {
  if (!InRange(Audio_Seq_unusedArtsStatus.artsLearned, category)) {
    VS_LOG_WARN("battle.ability", "arts of category {} out of range", category);
    return 0;
  }
  return Audio_Seq_unusedArtsStatus.artsLearned[category];
}

int LiveAbilityHost::WeaponCategoryKills(int category) const {
  if (!InRange(Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills,
               category)) {
    VS_LOG_WARN("battle.ability", "kills of category {} out of range",
                category);
    return 0;
  }
  return Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[category];
}

int LiveAbilityHost::BreakArtRequirement(int category, int tier) const {
  if (!InRange(Battle_breakArtPointRequirements, category) ||
      !InRange(Battle_breakArtPointRequirements[0], tier)) {
    VS_LOG_WARN("battle.ability", "break art requirement {}/{} out of range",
                category, tier);
    return 0;
  }
  return Battle_breakArtPointRequirements[category][tier];
}

int LiveAbilityHost::BattleAbilityRequirement(int index) const {
  if (!InRange(Battle_battleAbilityPointRequirements, index)) {
    VS_LOG_WARN("battle.ability", "ability requirement {} out of range", index);
    return 0;
  }
  return Battle_battleAbilityPointRequirements[index];
}

bool HasActionState(const char* caller) {
  if (Battle_actionState != nullptr) return true;
  VS_LOG_WARN("battle.ability", "{} without an action state", caller);
  return false;
}

bool HasCharacterState(const char* caller) {
  if (Battle_characterState != nullptr) return true;
  VS_LOG_WARN("battle.ability", "{} without a character state", caller);
  return false;
}

int PackPendingUnlocks(battle::PendingAbilityUnlocks pending) {
  return (pending.break_art ? 1 << kPendingBreakArtShift : 0) |
         (pending.battle_ability ? 1 : 0);
}

// The original traces the acting actor's animation every timing update
// (Core_Debugf "combat-flow").
void TraceAbilityTiming(const Battle_ActionState& state) {
  const Battle_ActionRequest& queued =
      state.queue[state.queueIndex & kQueueSlotMask];
  const int actor = queued.primaryTarget.target.targetActor;
  const Actors_ActorRecord* record = InRange(Actors_actorRecords, actor)
                                         ? Actors_actorRecords[actor]
                                         : nullptr;
  if (record == nullptr) return;
  VS_LOG_DEBUG("combat-flow",
               "animation id={} state={} active={} held={} timer={} "
               "window={} effects={} action={}",
               static_cast<unsigned>(record->animationId),
               static_cast<unsigned>(record->instance.actionAnimationState),
               static_cast<unsigned>(record->instance.currentAnimation),
               static_cast<unsigned>(record->pose.holdingLastFrame),
               static_cast<int>(state.abilityTimer),
               static_cast<unsigned>(state.abilityWindow),
               static_cast<unsigned>(Effects_effectBusyFlags),
               static_cast<unsigned>(queued.actionId));
}
}  // namespace

namespace VS::Battle {
void LiveArmDefenseAbilityTimingWindow(int target_actor_slot) {
  if (!HasActionState("arm defense window")) return;
  LiveAbilityHost host(Battle_actionState);
  ArmDefenseAbilityTimingWindow(target_actor_slot, host);
}

int LivePendingBattleAbilityUnlocks() {
  if (!HasCharacterState("pending ability unlocks")) return 0;
  const LiveAbilityHost host(Battle_actionState);
  return PackPendingUnlocks(PendingBattleAbilityUnlocks(host));
}

void LiveBeginPendingAbilityReward() {
  if (!HasActionState("pending ability reward")) return;
  LiveAbilityHost host(Battle_actionState);
  BeginPendingAbilityReward(host);
}

void LiveGrantPendingAbilityUnlock() {
  if (!HasActionState("grant ability unlock")) return;
  LiveAbilityHost host(Battle_actionState);
  GrantPendingAbilityUnlock(host);
}

int LiveIsActionUnlocked(unsigned action_id) {
  const LiveAbilityHost host(Battle_actionState);
  return IsActionUnlocked(action_id, host) ? 1 : 0;
}

void LiveUnlockAction(unsigned action_id) {
  LiveAbilityHost host(Battle_actionState);
  UnlockAction(action_id, host);
}

int LiveHasUnlockedSpellClass(int spell_class) {
  const LiveAbilityHost host(Battle_actionState);
  return HasUnlockedSpellClass(spell_class, host) ? 1 : 0;
}

int LiveHasUnlockedWeaponBreakArt() {
  if (!HasCharacterState("break art unlocks")) return 0;
  const LiveAbilityHost host(Battle_actionState);
  return HasUnlockedWeaponBreakArt(host) ? 1 : 0;
}

int LiveHasUnlockedBattleAbility(int defense) {
  const LiveAbilityHost host(Battle_actionState);
  return HasUnlockedBattleAbility(defense, host) ? 1 : 0;
}

void LiveAdvanceBattleAbilitySequence() {
  if (!HasActionState("advance ability sequence")) return;
  LiveAbilityHost host(Battle_actionState);
  AdvanceBattleAbilitySequence(host);
}

int LiveCanTriggerBattleAbility(int ability_input_slot) {
  if (!HasActionState("ability trigger check")) return 0;
  const LiveAbilityHost host(Battle_actionState);
  return CanTriggerBattleAbility(ability_input_slot, host) ? 1 : 0;
}

void LiveUpdateBattleAbilityTiming() {
  if (!HasActionState("ability timing")) return;
  TraceAbilityTiming(*Battle_actionState);
  LiveAbilityHost host(Battle_actionState);
  UpdateBattleAbilityTiming(host);
}
}  // namespace VS::Battle
