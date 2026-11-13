#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Field/GameMode.hpp"

namespace VS::Menu::Framework {
enum class LootListId : std::uintptr_t;
}

namespace VS::Battle {
enum class RewardPhase : std::uint32_t {
  kResume = 2,
  kResults = 3,
  kLoot = 4,
  kUnlockEffects = 5,
  kUnlockDialog = 6,
};

struct RewardSelection {
  std::uint16_t action_id = 0;
  std::uint32_t targets_position = 0;
  std::uint8_t continued = 0;
  std::uint8_t primary_actor = 0;
};

// Reads resolve their current owner on each call, including after presentation
// callbacks. No actor record is needed to inspect a selected action.
class RewardWorld {
 public:
  virtual ~RewardWorld() = default;
  virtual void SetPhase(RewardPhase phase) = 0;
  virtual void SetInteractionPaused(bool paused) = 0;
  [[nodiscard]] virtual std::int32_t FollowAction() = 0;
  [[nodiscard]] virtual std::uint8_t ScriptFlag(int flag) = 0;
  [[nodiscard]] virtual std::uint32_t PendingUnlocks() = 0;
  [[nodiscard]] virtual RewardSelection Selection() = 0;
  [[nodiscard]] virtual bool DefinitionUnlocked(std::uint16_t action) = 0;
  virtual void MarkDefinitionUnlocked(std::uint16_t action) = 0;

  virtual void RoomPalettes(const Field::ColorTransform& transform) = 0;
  virtual void SceneColors(const Field::ColorTransform& transform) = 0;
  virtual void EnableClutAnimation(bool enabled) = 0;
  virtual void FreezeActorAnimations() = 0;
  virtual void UnfreezeActorAnimations() = 0;
  virtual void PauseRoomAnimations(bool paused) = 0;
  virtual void ShowMinimap(bool visible) = 0;
  virtual void DuckAmbience(bool ducked) = 0;
  virtual void RestoreIdlePose() = 0;
  virtual void PlayLearnedPose() = 0;
  virtual void SpawnLearnedEffect() = 0;
  virtual void PauseMovingPlatforms() = 0;
  virtual void ShowActionLearned(std::uint16_t action) = 0;
  virtual void OpenAbilityUnlocks(std::uint16_t art, std::uint16_t ability) = 0;
  [[nodiscard]] virtual Menu::Framework::LootListId LootHead() = 0;
  virtual void OpenLoot(Menu::Framework::LootListId owner) = 0;
  virtual void OpenResults() = 0;
  virtual void UpdateMenu() = 0;
  [[nodiscard]] virtual std::int16_t MenuStatus() = 0;
  virtual void ReleaseMenuRows() = 0;
};

// Older ability rewards have distinct recovery policies from reward unlocks.
class RewardDialogWorld : public RewardWorld {
 public:
  [[nodiscard]] virtual std::uint32_t EffectsBusyFlags() = 0;
  [[nodiscard]] virtual std::int32_t SceneNotificationPending() = 0;
  [[nodiscard]] virtual std::int32_t LootMenuOpened() = 0;
  virtual void ResumeFieldAfterMenu() = 0;
  virtual void ReleaseSceneNotification() = 0;
  virtual void BeginPendingAbilityReward() = 0;
  virtual void GrantPendingAbilityUnlock() = 0;
};

void BeginRewardPause(RewardWorld& world);
void EndRewardPause(RewardWorld& world);
void ResumeAfterRewards(RewardWorld& world);
void CheckRewardUnlocks(RewardWorld& world);
void ShowPendingRewardUnlock(RewardWorld& world);
void OpenPendingLootRewards(RewardWorld& world);
void OpenRewardResults(RewardWorld& world);
void UpdateRewardResultsDialog(RewardWorld& world);
void UpdateLootResults(RewardDialogWorld& world);
void UpdateAbilityUnlockDelay(RewardDialogWorld& world);
void UpdateAbilityUnlockResults(RewardDialogWorld& world);
void UpdatePendingLootDialog(RewardDialogWorld& world);
}  // namespace VS::Battle
