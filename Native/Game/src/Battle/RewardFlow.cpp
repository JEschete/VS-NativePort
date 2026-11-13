// Hand-maintained (tools/port/owned.csv).
#include "Battle/RewardFlow.hpp"

namespace VS::Battle {
namespace {
constexpr int kUnlockStoryFlag = 0xB6;
constexpr int kRewardStoryFlag = 0xAB;
constexpr int kResultsStage = 3;
constexpr int kPendingArtShift = 16;
constexpr int kMenuBusy = 1;

bool PendingAction(RewardWorld& world) {
  const RewardSelection selection = world.Selection();
  return selection.action_id != 0 &&
         !world.DefinitionUnlocked(selection.action_id) &&
         selection.targets_position == 0 && selection.continued == 0 &&
         selection.primary_actor == 0;
}

void BeginUnlockEffects(RewardWorld& world) {
  world.SetPhase(RewardPhase::kUnlockEffects);
  if (world.FollowAction() != 0) {
    world.PlayLearnedPose();
    world.SpawnLearnedEffect();
  }
  world.PauseMovingPlatforms();
}
}  // namespace

// decomp: BATTLE.PRG func_8007B410 (src/BATTLE/BATTLE.PRG/146C.c:8287)
void BeginRewardPause(RewardWorld& world) {
  world.RoomPalettes(Field::kMenuRoomPaletteTransform);
  world.EnableClutAnimation(false);
  world.UnfreezeActorAnimations();
  world.PauseRoomAnimations(true);
  world.ShowMinimap(false);
  world.DuckAmbience(true);
  world.SetInteractionPaused(true);
}

// decomp: BATTLE.PRG func_8007B470 (src/BATTLE/BATTLE.PRG/146C.c:8298)
void EndRewardPause(RewardWorld& world) {
  world.EnableClutAnimation(true);
  world.SceneColors(Field::kResumeSceneColorTransform);
  world.FreezeActorAnimations();
  world.PauseRoomAnimations(false);
  world.DuckAmbience(false);
  world.SetInteractionPaused(false);
}

// decomp: BATTLE.PRG func_8007B4C4 (src/BATTLE/BATTLE.PRG/146C.c:8308)
void ResumeAfterRewards(RewardWorld& world) {
  world.SetPhase(RewardPhase::kResume);
  EndRewardPause(world);
  if (world.FollowAction() != 0) world.RestoreIdlePose();
}

// decomp: BATTLE.PRG func_8007B508 (src/BATTLE/BATTLE.PRG/146C.c:8317)
void CheckRewardUnlocks(RewardWorld& world) {
  const bool check_abilities = world.ScriptFlag(kUnlockStoryFlag) == 0 ||
                               world.ScriptFlag(kUnlockStoryFlag) == 2 ||
                               world.ScriptFlag(kUnlockStoryFlag) == 3;
  if ((check_abilities && world.PendingUnlocks() != 0) ||
      PendingAction(world)) {
    BeginUnlockEffects(world);
    return;
  }
  ResumeAfterRewards(world);
}

// decomp: BATTLE.PRG func_8007B63C (src/BATTLE/BATTLE.PRG/146C.c:8346)
void ShowPendingRewardUnlock(RewardWorld& world) {
  if (PendingAction(world)) {
    world.SetPhase(RewardPhase::kUnlockDialog);
    BeginRewardPause(world);
    world.ShowActionLearned(world.Selection().action_id);
    // Both callbacks can replace the selection, including with definition 0.
    world.MarkDefinitionUnlocked(world.Selection().action_id);
    return;
  }
  const std::uint32_t pending = world.PendingUnlocks();
  world.SetPhase(RewardPhase::kUnlockDialog);
  BeginRewardPause(world);
  world.OpenAbilityUnlocks(
      static_cast<std::uint16_t>(pending >> kPendingArtShift),
      static_cast<std::uint16_t>(pending));
}

// decomp: BATTLE.PRG func_8007B764 (src/BATTLE/BATTLE.PRG/146C.c:8366)
void OpenPendingLootRewards(RewardWorld& world) {
  if (world.ScriptFlag(kRewardStoryFlag) >= kResultsStage) {
    const auto owner = world.LootHead();
    if (owner != Menu::Framework::LootListId{}) {
      world.OpenLoot(owner);
      world.SetPhase(RewardPhase::kLoot);
      BeginRewardPause(world);
      return;
    }
    CheckRewardUnlocks(world);
    return;
  }
  ResumeAfterRewards(world);
}

// decomp: BATTLE.PRG func_8007B7DC (src/BATTLE/BATTLE.PRG/146C.c:8381)
void OpenRewardResults(RewardWorld& world) {
  if (world.ScriptFlag(kRewardStoryFlag) >= kResultsStage) {
    world.OpenResults();
    world.SetPhase(RewardPhase::kResults);
    BeginRewardPause(world);
    return;
  }
  OpenPendingLootRewards(world);
}

// decomp: BATTLE.PRG func_8007B834 (src/BATTLE/BATTLE.PRG/146C.c:8392)
void UpdateRewardResultsDialog(RewardWorld& world) {
  world.UpdateMenu();
  if (world.MenuStatus() != kMenuBusy) {
    EndRewardPause(world);
    world.ReleaseMenuRows();
    OpenPendingLootRewards(world);
  }
}
}  // namespace VS::Battle
