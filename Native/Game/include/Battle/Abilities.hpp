#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Typed battle abilities (#276 row 31): the chain and defense ability timing
// window, the follow-up sequence an accepted input starts, break art and
// battle ability unlocks earned from kills, and the unlocked-action queries
// the menus use. Every action-state field, actor, progress counter and
// presentation call goes through an AbilityHost.
namespace VS::Battle {
// The ability buttons a chain or defense input can use.
inline constexpr int kAbilityInputSlots = 3;
// The highest action id the unlock queries accept.
inline constexpr std::uint32_t kLastUnlockableAction = 255;

// The action-state fields the ability timing reads and writes. The host
// keeps each field's original width.
enum class AbilityField : std::uint8_t {
  kPreviousChainButtons,
  kChainAbilitySlot,
  kPreviousChainInput,
  kAbilityInputSucceeded,
  kAbilityInputAttempted,
  kChainContinued,
  kAbilityTimer,
  kAbilityWindow,
};

// The two mapped ability rows.
enum class AbilityKind : std::uint8_t { kChain, kDefense };

// One action request (the queued step or the selection), as read.
struct ActionRequestView {
  int action_id = 0;
  int primary_actor = 0;
  bool primary_targets_position = false;
  int target_count = 0;
  int first_target_actor = 0;
  bool first_target_targets_position = false;
};

// One target of the selection, as the defense collapse reads it.
struct SelectionTargetView {
  int actor = 0;
  int hp = 0;
  int mp = 0;
  // Whether the hit changes hp or mp.
  bool has_vital_effect = false;
};

// The unlocks the current kill counts have earned.
struct PendingAbilityUnlocks {
  bool break_art = false;
  bool battle_ability = false;

  [[nodiscard]] bool Any() const { return break_art || battle_ability; }
};

class AbilityHost {
 public:
  virtual ~AbilityHost() = default;

  // Action state.
  [[nodiscard]] virtual std::uint32_t QueueIndex() const = 0;
  virtual void SetQueueIndex(std::uint32_t index) = 0;
  [[nodiscard]] virtual std::uint32_t ChainStep() const = 0;
  virtual void SetChainStep(std::uint32_t step) = 0;
  [[nodiscard]] virtual int Field(AbilityField field) const = 0;
  virtual void SetField(AbilityField field, int value) = 0;
  // The queued request of the current step (queue[queueIndex & 3]).
  [[nodiscard]] virtual ActionRequestView QueuedAction() const = 0;
  [[nodiscard]] virtual ActionRequestView Selection() const = 0;
  [[nodiscard]] virtual int TickSpeed() const = 0;
  virtual void SetTickSpeed(int speed) = 0;
  virtual void SetActionFlowMode(int mode) = 0;
  [[nodiscard]] virtual int RecoveryRewardMode() const = 0;

  // The defense collapse of the selection.
  [[nodiscard]] virtual SelectionTargetView SelectionTarget(
      int index) const = 0;
  // Copies the whole target record over the primary target.
  virtual void CopySelectionTargetToPrimary(int index) = 0;
  virtual void SetSelectionPrimaryVitals(int hp, int mp) = 0;
  // Keeps a copy of the primary target record, then restores it as the first
  // target aimed at the given limb.
  virtual void SavePrimaryTarget() = 0;
  virtual void RestoreSavedTargetAsFirst(int limb) = 0;
  virtual void SetSelectionTargetCount(int count) = 0;
  // Centres the selection on the actor's placement.
  virtual void FocusSelectionOnActor(int actor) = 0;

  // Mapped abilities, action definitions and actors.
  [[nodiscard]] virtual int MappedAbility(AbilityKind kind,
                                          int index) const = 0;
  // Whether the player can pay for and use the action now.
  [[nodiscard]] virtual bool ActionAvailable(int action_id) const = 0;
  [[nodiscard]] virtual bool ActionDefinitionUnlocked(int action_id) const = 0;
  virtual void SetActionDefinitionUnlocked(int action_id) = 0;
  [[nodiscard]] virtual int ActorRoomSpawnIndex(int actor) const = 0;
  [[nodiscard]] virtual int ActorHp(int actor) const = 0;
  [[nodiscard]] virtual int ActorDefaultTargetLimb(int actor) const = 0;
  [[nodiscard]] virtual int SpellClassCount(int spell_class) const = 0;
  [[nodiscard]] virtual int SpellClassSpellId(int spell_class,
                                              int index) const = 0;
  [[nodiscard]] virtual int AbilityActionId(AbilityKind kind,
                                            int index) const = 0;

  // Progress.
  [[nodiscard]] virtual int EquippedWeaponCategory() const = 0;
  [[nodiscard]] virtual int ArtsLearned(int category) const = 0;
  [[nodiscard]] virtual int WeaponCategoryKills(int category) const = 0;
  [[nodiscard]] virtual int BreakArtRequirement(int category,
                                                int tier) const = 0;
  [[nodiscard]] virtual int BattleAbilitiesUnlocked() const = 0;
  [[nodiscard]] virtual int BattleAbilityRequirement(int index) const = 0;
  [[nodiscard]] virtual int TotalKills() const = 0;
  virtual void SetTotalKills(int kills) = 0;
  [[nodiscard]] virtual int MaxChain() const = 0;
  virtual void SetMaxChain(int chain) = 0;
  [[nodiscard]] virtual int ChainStreak() const = 0;
  virtual void SetChainStreak(int streak) = 0;
  [[nodiscard]] virtual int ScriptFlag(int flag) const = 0;

  // Sequencing and presentation.
  virtual void WaitForPostActionMovement() = 0;
  virtual void FinishBattleAbilitySequence() = 0;
  virtual void PlayAbilityLearnedPose() = 0;
  virtual void SpawnAbilityLearnedEffect() = 0;
  virtual void PauseMovingPlatforms() = 0;
  virtual void PauseFieldForMenu() = 0;
  virtual void ShowActionLearnedNotification(int action_id) = 0;
  virtual void OpenAbilityUnlockMenu(PendingAbilityUnlocks unlocks) = 0;
  virtual void ReleaseActorSequence(int actor, int kind) = 0;
  // Tells the AI about the queued step's relationship changes.
  virtual void NotifyQueuedActionRelationships() = 0;
  // Resolves the action against the selection into the queued step.
  virtual void ResolveSelectionIntoQueue(int action_id) = 0;
  virtual void QueueChainCount(int count) = 0;
  virtual void ShowAbilityTimingCue(int frames) = 0;
  virtual void QueueAbilityTimingResult(int result) = 0;
  virtual void ClearAbilityInputResult() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() const = 0;
  [[nodiscard]] virtual bool EffectsBusy() const = 0;
  [[nodiscard]] virtual bool IsActorActionAnimationBusy(int actor) const = 0;
  virtual void RestoreActorIdlePose(int actor) = 0;
};

// Opens the ability timing window for an attack on the player.
void ArmDefenseAbilityTimingWindow(int target_actor_slot, AbilityHost& host);
[[nodiscard]] PendingAbilityUnlocks PendingBattleAbilityUnlocks(
    const AbilityHost& host);
void BeginPendingAbilityReward(AbilityHost& host);
void GrantPendingAbilityUnlock(AbilityHost& host);
void AdvanceBattleAbilitySequence(AbilityHost& host);
// Whether an ability input can trigger now; slot 0 asks whether any ability
// is mapped, slots 1..3 name the button's ability.
[[nodiscard]] bool CanTriggerBattleAbility(int ability_input_slot,
                                           const AbilityHost& host);
void UpdateBattleAbilityTiming(AbilityHost& host);
[[nodiscard]] bool IsActionUnlocked(std::uint32_t action_id,
                                    const AbilityHost& host);
void UnlockAction(std::uint32_t action_id, AbilityHost& host);
[[nodiscard]] bool HasUnlockedSpellClass(int spell_class,
                                         const AbilityHost& host);
[[nodiscard]] bool HasUnlockedWeaponBreakArt(const AbilityHost& host);
[[nodiscard]] bool HasUnlockedBattleAbility(int defense,
                                            const AbilityHost& host);
}  // namespace VS::Battle
