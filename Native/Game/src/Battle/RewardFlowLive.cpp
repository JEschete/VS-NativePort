// Hand-maintained (tools/port/owned.csv).
#include "Battle/RewardFlowLive.hpp"

#include <cstdlib>
#include <iterator>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/AmbienceLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/RewardFlow.hpp"
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

class LiveWorld final : public RewardWorld {
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
}  // namespace VS::Battle
