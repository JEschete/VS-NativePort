// Hand-maintained (tools/port/owned.csv).
#include "Battle/RewardFlowLive.hpp"

#include <cstdlib>
#include <iterator>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/AmbienceLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/RewardFlow.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Rewards.h"
#include "Battle/TargetingState.h"
#include "Core/Diagnostics.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/GameModeLiveState.h"
#include "Field/PlatformPlacementLive.h"
#include "Items/LootLiveState.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/TextBox.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid, const char* reason) {
  if (!valid) {
    Core_Errorf("battle", "Reward flow refused: %s", reason);
    std::abort();
  }
}

Battle_ActionState& CurrentAction() {
  Require(Battle_actionState != nullptr, "missing action state");
  return *Battle_actionState;
}

Battle_ActionDefinition& Definition(std::uint16_t action) {
  Require(action < std::size(Battle_actionDefinitions),
          "action definition outside table");
  return Battle_actionDefinitions[action];
}

class LiveWorld final : public RewardDispatchWorld {
 public:
  void SetPhase(RewardPhase phase) override {
    Battle_actionFlowMode = static_cast<std::uint32_t>(phase);
  }
  void SetInteractionPaused(bool paused) override {
    Field_isInteractionPaused = paused ? 1 : 0;
  }
  std::int32_t FollowAction() override { return Battle_rewardsFollowAction; }
  std::uint8_t ScriptFlag(int flag) override {
    return State::LiveScriptFlag(flag);
  }
  std::uint32_t PendingUnlocks() override {
    return static_cast<std::uint32_t>(LivePendingBattleAbilityUnlocks());
  }
  RewardSelection Selection() override {
    const auto& state = CurrentAction();
    return {.action_id = state.selection.actionId,
            .targets_position = static_cast<std::uint32_t>(
                state.selection.primaryTarget.targetsPosition),
            .continued = static_cast<std::uint8_t>(state.chainContinued),
            .primary_actor = state.selection.primaryTarget.target.targetActor};
  }
  bool DefinitionUnlocked(std::uint16_t action) override {
    return Definition(action).unlocked != 0;
  }
  void MarkDefinitionUnlocked(std::uint16_t action) override {
    Definition(action).unlocked = 1;
  }
  void RoomPalettes(const Field::ColorTransform& transform) override {
    Render::TransformLiveRoomPalettes(
        transform.mode, transform.duration, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  void SceneColors(const Field::ColorTransform& transform) override {
    Render::TransformLiveSceneColors(
        transform.mode, transform.duration, transform.red_offset,
        transform.green_offset, transform.blue_offset);
  }
  void EnableClutAnimation(bool enabled) override {
    Render::SetLiveClutAnimationEnabled(enabled ? 1 : 0);
  }
  void FreezeActorAnimations() override { Actors::FreezeLiveActorAnimations(); }
  void UnfreezeActorAnimations() override {
    Actors::UnfreezeLiveActorAnimations();
  }
  void PauseRoomAnimations(bool paused) override {
    Field::LiveSetRoomAnimationsPaused(paused ? 1 : 0);
  }
  void ShowMinimap(bool visible) override {
    Field::SetLiveMinimapVisible(visible ? 1 : 0);
  }
  void DuckAmbience(bool ducked) override {
    Audio::LiveSetRoomAmbienceDucked(ducked ? 1 : 0);
  }
  void RestoreIdlePose() override { Actors::RestoreLiveActorIdlePose(0); }
  void PlayLearnedPose() override {
    constexpr int kLearnedPose = 6;
    Actors::PlayLiveActorActionPose(0, nullptr, kLearnedPose);
  }
  void SpawnLearnedEffect() override { Effects_SpawnAbilityLearnedEffect(); }
  void PauseMovingPlatforms() override {
    Field_SetMovingPlatformsPausedEntry(1);
  }
  void ShowActionLearned(std::uint16_t action) override {
    constexpr int kLearnedNotice = 0xC;
    Menu_ShowSceneNotification(kLearnedNotice, action, 1);
  }
  void OpenAbilityUnlocks(std::uint16_t art, std::uint16_t ability) override {
    Menu::Framework::OpenLiveAbilityUnlockMenu(art, ability);
  }
  Menu::Framework::LootListId LootHead() override {
    return Menu::Framework::LootListId{
        reinterpret_cast<std::uintptr_t>(Items_lootListHead)};
  }
  void OpenLoot(Menu::Framework::LootListId owner) override {
    Menu::Framework::OpenLiveLootListMenu(
        reinterpret_cast<Items_LootListNode*>(owner));
  }
  void OpenResults() override { Menu::Framework::OpenLiveChallengeMenu(); }
  void UpdateMenu() override { Menu::Framework::UpdateLiveMenuOverlay(); }
  std::int16_t MenuStatus() override {
    const auto* owner = Menu::Framework::LiveActionSelectionState();
    Require(owner != nullptr, "missing menu selection state");
    return owner->menuStatus;
  }
  void ReleaseMenuRows() override { Menu::Framework::ReleaseLiveMenuRows(); }
  std::uint32_t EffectsBusyFlags() override {
    return static_cast<std::uint32_t>(Effects_effectBusyFlags);
  }
  std::int32_t SceneNotificationPending() override {
    return Menu_IsSceneNotificationPending();
  }
  std::int32_t LootMenuOpened() override { return Battle_lootMenuOpened; }
  void ResumeFieldAfterMenu() override { Field::LiveResumeFieldAfterMenu(); }
  void ReleaseSceneNotification() override { Menu_ReleaseSceneNotification(); }
  void BeginPendingAbilityReward() override { LiveBeginPendingAbilityReward(); }
  void GrantPendingAbilityUnlock() override { LiveGrantPendingAbilityUnlock(); }
  std::uint32_t CurrentPhase() override { return Battle_actionFlowMode; }
  void SetLootMenuOpened(std::int32_t value) override {
    Battle_lootMenuOpened = value;
  }
  void SetFollowAction(std::int32_t value) override {
    Battle_rewardsFollowAction = value;
  }
};
}  // namespace

void BeginLiveRewardPause() {
  LiveWorld world;
  BeginRewardPause(world);
}
void EndLiveRewardPause() {
  LiveWorld world;
  EndRewardPause(world);
}
void ResumeLiveAfterRewards() {
  LiveWorld world;
  ResumeAfterRewards(world);
}
void CheckLiveRewardUnlocks() {
  LiveWorld world;
  CheckRewardUnlocks(world);
}
void ShowLivePendingRewardUnlock() {
  LiveWorld world;
  ShowPendingRewardUnlock(world);
}
void OpenLivePendingLootRewards() {
  LiveWorld world;
  OpenPendingLootRewards(world);
}
void OpenLiveRewardResults() {
  LiveWorld world;
  OpenRewardResults(world);
}
void UpdateLiveRewardResultsDialog() {
  LiveWorld world;
  UpdateRewardResultsDialog(world);
}
// decomp: BATTLE.PRG func_800797BC (src/BATTLE/BATTLE.PRG/146C.c:7516)
void UpdateLiveLootResults() {
  LiveWorld world;
  UpdateLootResults(world);
}
// decomp: BATTLE.PRG func_8007980C (src/BATTLE/BATTLE.PRG/146C.c:7526)
void UpdateLiveAbilityUnlockDelay() {
  LiveWorld world;
  UpdateAbilityUnlockDelay(world);
}
// decomp: BATTLE.PRG func_8007983C (src/BATTLE/BATTLE.PRG/146C.c:7533)
void UpdateLiveAbilityUnlockResults() {
  LiveWorld world;
  UpdateAbilityUnlockResults(world);
}
// decomp: BATTLE.PRG func_8007B884 (src/BATTLE/BATTLE.PRG/146C.c:8402)
void UpdateLivePendingLootDialog() {
  LiveWorld world;
  UpdatePendingLootDialog(world);
}
// decomp: BATTLE.PRG func_8007B908 (src/BATTLE/BATTLE.PRG/146C.c:8418)
void WaitForLiveRewardUnlockEffects() {
  LiveWorld world;
  WaitForRewardUnlockEffects(world);
}
// decomp: BATTLE.PRG func_8007B938 (src/BATTLE/BATTLE.PRG/146C.c:8425)
void UpdateLiveRewardUnlockDialog() {
  LiveWorld world;
  UpdateRewardUnlockDialog(world);
}
// decomp: BATTLE.PRG func_8007B9A0 (src/BATTLE/BATTLE.PRG/146C.c:8436)
void BeginLiveEventRewardResults() {
  LiveWorld world;
  BeginEventRewardResults(world);
}
// decomp: BATTLE.PRG func_8007B9CC (src/BATTLE/BATTLE.PRG/146C.c:8443)
void BeginLivePostActionRewards() {
  LiveWorld world;
  BeginPostActionRewards(world);
}
// decomp: BATTLE.PRG func_8007B9FC (src/BATTLE/BATTLE.PRG/146C.c:8450)
int UpdateLiveRewardSequence() {
  LiveWorld world;
  return UpdateRewardSequence(world);
}
}  // namespace VS::Battle
