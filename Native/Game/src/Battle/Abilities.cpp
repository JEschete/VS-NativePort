// Hand-maintained (tools/port/owned.csv).
#include "Battle/Abilities.hpp"  // NOLINT(build/include): typed API

#include <cstdint>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
using Core::Math::R3000Quotient;
using Core::Math::WrappingSubtract;

// Pad bits of the three ability buttons (the original PADRleft, PADRup and
// PADRright), and the input slot each one selects.
constexpr std::uint32_t kSquareButton = 1u << 7;
constexpr std::uint32_t kTriangleButton = 1u << 4;
constexpr std::uint32_t kCircleButton = 1u << 5;
constexpr std::uint32_t kAbilityButtons(kSquareButton | kTriangleButton |
                                        kCircleButton);
constexpr int kSquareSlot = 3;
constexpr int kTriangleSlot = 2;
constexpr int kCircleSlot = 1;

// abilityInputSucceeded: which follow-up an accepted input starts.
constexpr int kNoFollowUp = 0;
constexpr int kChainFollowUp = 1;
constexpr int kDefenseFollowUp = 2;

// Ability timing results (Battle_QueueAbilityTimingResult).
constexpr int kTimingEarly = 0;
constexpr int kTimingNearlyEarly = 1;
constexpr int kTimingHit = 2;
constexpr int kTimingLate = 3;
constexpr int kTimingMissed = 4;

// Action flow modes.
constexpr int kFlowAbilityStep = 8;
constexpr int kFlowAbilityLearned = 0xD;
constexpr int kFlowAbilityUnlockMenu = 0xE;

// Recovery reward modes.
constexpr int kRewardWait = 0;
constexpr int kRewardCheckUnlocks = 2;

// Timing window margins in ticks.
constexpr int kFastTimingFlag = 1;
constexpr int kFastMargin = 8;
constexpr int kNormalMargin = 10;
constexpr int kHeavyActionMargin = 4;
constexpr int kHeavyDefenseAction = 54;
constexpr std::uint32_t kStepsPerMarginTick = 3;
constexpr int kMinimumMargin = 2;

// Action id ranges.
constexpr int kFirstAbilityAction = 40;
constexpr unsigned kAbilityActionCount = 14;
constexpr int kFirstSpellSequenceAction = 0x8D;
constexpr int kFirstBreakArtAction = 0xB8;
constexpr int kEndBreakArtAction = 0xE0;
constexpr int kSpellSequenceKind = 5;
constexpr int kBreakArtSequenceKind = 4;
constexpr int kBreakArtsPerCategory = 4;

// The sequence ends after this many steps.
constexpr std::uint32_t kMaxChainSteps = 999;
constexpr int kChainTickSpeed = 2;

// An actor whose spawn slot is this has left the room.
constexpr int kDetachedSpawnIndex = 0x80;

// Kill and unlock progress.
constexpr int kWeaponCategories = 10;
constexpr int kBreakArtTiers = 4;
constexpr int kBattleAbilityUnlocks = 22;
constexpr int kMaxKills = 0xFFFF;
constexpr int kMaxChainStreak = 0xFFFE;
constexpr int kChainKillFlag = 0x82;
constexpr int kChainKillThreshold = 10;

// The cue lead time is twice the margin, in frames of the current speed.
constexpr int kTimingCueMultiplier = 2;

AbilityKind KindFor(const ActionRequestView& action) {
  return action.primary_actor == 0 ? AbilityKind::kChain
                                   : AbilityKind::kDefense;
}

bool HasMappedAbility(AbilityKind kind, const AbilityHost& host) {
  bool mapped = false;
  for (int index = 0; index < kAbilityInputSlots; ++index) {
    if (host.MappedAbility(kind, index) != 0) mapped = true;
  }
  return mapped;
}

bool ValidAbilitySlot(int slot) {
  return slot >= 1 && slot <= kAbilityInputSlots;
}

// The selected action is new to the player and is learned on its own.
bool IsNewActionLearned(const AbilityHost& host) {
  const ActionRequestView selection = host.Selection();
  return selection.action_id != 0 &&
         !host.ActionDefinitionUnlocked(selection.action_id) &&
         !selection.primary_targets_position &&
         host.Field(AbilityField::kChainContinued) == 0 &&
         selection.primary_actor == 0;
}

void BeginAbilityLearnedPose(AbilityHost& host) {
  host.SetActionFlowMode(kFlowAbilityLearned);
  host.PlayAbilityLearnedPose();
  host.SpawnAbilityLearnedEffect();
  host.PauseMovingPlatforms();
}

void CountChainKill(AbilityHost& host) {
  if (host.BattleAbilitiesUnlocked() < kBattleAbilityUnlocks &&
      host.TotalKills() < kMaxKills) {
    host.SetTotalKills(host.TotalKills() + 1);
  }
}

void MarkInputAttempted(AbilityHost& host, int follow_up) {
  host.SetField(AbilityField::kAbilityInputSucceeded, follow_up);
  host.SetField(AbilityField::kAbilityInputAttempted, 1);
}

void ContinueChain(const ActionRequestView& action, AbilityHost& host) {
  const int slot = host.Field(AbilityField::kChainAbilitySlot);
  if (host.ActorHp(action.first_target_actor) != 0 && ValidAbilitySlot(slot)) {
    host.NotifyQueuedActionRelationships();
    host.SetActionFlowMode(kFlowAbilityStep);
    host.SetChainStep(host.ChainStep() + 1);
    host.SetQueueIndex(host.QueueIndex() + 1);
    host.ResolveSelectionIntoQueue(
        host.MappedAbility(AbilityKind::kChain, slot - 1));
    host.QueueChainCount(static_cast<int>(host.QueueIndex()));
    if (static_cast<std::uint32_t>(host.MaxChain()) < host.QueueIndex()) {
      host.SetMaxChain(static_cast<int>(host.QueueIndex()));
    }
  } else {
    if (!ValidAbilitySlot(slot)) {
      VS_LOG_WARN("battle.ability", "chain step with invalid slot={}", slot);
    }
    host.FinishBattleAbilitySequence();
  }
  if (host.ScriptFlag(kChainKillFlag) >= kChainKillThreshold) {
    CountChainKill(host);
  }
}

// The defense follow-up aims at the attacker alone: the hit that carried the
// damage becomes the primary target with the attacker's summed hp and mp,
// and the original primary target becomes the only target.
void CollapseDefenseSelection(AbilityHost& host) {
  const int saved_actor = host.Selection().primary_actor;
  host.SavePrimaryTarget();
  const int count = host.Selection().target_count;
  int chosen = 0;
  for (int index = 0; index < count; ++index) {
    const SelectionTargetView target = host.SelectionTarget(index);
    if (target.actor == 0 && target.has_vital_effect) chosen = index;
  }
  host.CopySelectionTargetToPrimary(chosen);
  const int primary_actor = host.SelectionTarget(chosen).actor;
  int hp = 0;
  int mp = 0;
  for (int index = 0; index < count; ++index) {
    const SelectionTargetView target = host.SelectionTarget(index);
    if (target.actor == primary_actor) {
      hp += target.hp;
      mp += target.mp;
    }
  }
  host.SetSelectionPrimaryVitals(hp, mp);
  host.RestoreSavedTargetAsFirst(host.ActorDefaultTargetLimb(saved_actor));
  host.SetSelectionTargetCount(1);
  host.FocusSelectionOnActor(saved_actor);
}

void ContinueDefense(AbilityHost& host) {
  const int slot = host.Field(AbilityField::kChainAbilitySlot);
  if (!ValidAbilitySlot(slot)) {
    VS_LOG_WARN("battle.ability", "defense step with invalid slot={}", slot);
    host.FinishBattleAbilitySequence();
    return;
  }
  host.NotifyQueuedActionRelationships();
  host.SetActionFlowMode(kFlowAbilityStep);
  host.SetChainStep(host.ChainStep() + 1);
  CollapseDefenseSelection(host);
  host.SetQueueIndex(host.QueueIndex() + 1);
  host.SetField(AbilityField::kChainContinued, 1);
  host.ResolveSelectionIntoQueue(
      host.MappedAbility(AbilityKind::kDefense, slot - 1));
  CountChainKill(host);
}

int AbilityTimingMargin(const ActionRequestView& action,
                        const AbilityHost& host) {
  int margin =
      host.ScriptFlag(kFastTimingFlag) != 0 ? kFastMargin : kNormalMargin;
  if (action.primary_actor != 0 && action.action_id >= kHeavyDefenseAction) {
    margin += kHeavyActionMargin;
  }
  margin = WrappingSubtract(
      margin,
      static_cast<std::int32_t>(host.QueueIndex() / kStepsPerMarginTick));
  return margin < kMinimumMargin ? kMinimumMargin : margin;
}

int AbilityButtonSlot(std::uint32_t pressed) {
  if ((pressed & kSquareButton) != 0) return kSquareSlot;
  if ((pressed & kTriangleButton) != 0) return kTriangleSlot;
  return kCircleSlot;
}

bool IsAbilityButtonMapped(const ActionRequestView& action, int slot,
                           const AbilityHost& host) {
  return !action.primary_targets_position &&
         host.MappedAbility(KindFor(action), slot - 1) != 0;
}

// An input landed inside the window: accept it as the chain or defense
// follow-up when the button is fresh and differs from the previous input.
void AcceptTimedInput(const ActionRequestView& action, int slot,
                      AbilityHost& host) {
  const auto previous_buttons = static_cast<std::uint32_t>(
      host.Field(AbilityField::kPreviousChainButtons));
  if ((host.PressedButtons() & ~previous_buttons & kAbilityButtons) == 0) {
    return;
  }
  const bool fresh_slot = host.Field(AbilityField::kPreviousChainInput) != slot;
  if (action.primary_actor == 0) {
    if (fresh_slot && host.MappedAbility(AbilityKind::kChain, slot - 1) != 0) {
      host.SetField(AbilityField::kPreviousChainInput, slot);
      host.SetField(AbilityField::kChainAbilitySlot, slot);
      MarkInputAttempted(host, kChainFollowUp);
    } else {
      MarkInputAttempted(host, kNoFollowUp);
    }
    return;
  }
  if (action.target_count <= 0 || action.first_target_actor != 0 ||
      action.first_target_targets_position) {
    return;
  }
  if (fresh_slot && host.MappedAbility(AbilityKind::kDefense, slot - 1) != 0) {
    host.SetField(AbilityField::kPreviousChainInput, slot);
    host.SetField(AbilityField::kChainAbilitySlot, slot);
    MarkInputAttempted(host, kDefenseFollowUp);
    if (host.ChainStreak() <= kMaxChainStreak) {
      host.SetChainStreak(host.ChainStreak() + 1);
    }
  } else {
    MarkInputAttempted(host, kNoFollowUp);
  }
}

// Grades an input against the open window.
void JudgeTimedInput(const ActionRequestView& action, int slot, int margin,
                     int timer, AbilityHost& host) {
  const int window = host.Field(AbilityField::kAbilityWindow);
  if (window - margin >= timer) {
    host.QueueAbilityTimingResult(kTimingEarly);
    MarkInputAttempted(host, kNoFollowUp);
  } else if (window >= timer) {
    host.QueueAbilityTimingResult(kTimingNearlyEarly);
    MarkInputAttempted(host, kNoFollowUp);
  } else if (window + margin >= timer) {
    host.QueueAbilityTimingResult(kTimingHit);
    if (host.Field(AbilityField::kAbilityInputAttempted) == 0) {
      AcceptTimedInput(action, slot, host);
    }
  } else {
    host.QueueAbilityTimingResult(window + margin * 2 >= timer ? kTimingLate
                                                               : kTimingMissed);
    MarkInputAttempted(host, kNoFollowUp);
  }
  if (host.Field(AbilityField::kAbilityInputSucceeded) == kNoFollowUp) {
    host.ClearAbilityInputResult();
  }
}

// Handles an ability button press. False when the step was handed to the
// follow-up sequence and the update ends.
bool HandleAbilityInput(const ActionRequestView& action, int margin, int timer,
                        AbilityHost& host) {
  const int slot = AbilityButtonSlot(host.PressedButtons());
  if (!IsAbilityButtonMapped(action, slot, host)) return true;
  if (!CanTriggerBattleAbility(slot, host)) {
    if (host.Field(AbilityField::kAbilityInputAttempted) == 0) {
      MarkInputAttempted(host, kNoFollowUp);
    }
    return true;
  }
  if (host.Field(AbilityField::kAbilityWindow) == 0) {
    host.QueueAbilityTimingResult(kTimingNearlyEarly);
    host.SetField(AbilityField::kAbilityInputAttempted, 1);
    return true;
  }
  if (!action.primary_targets_position) {
    JudgeTimedInput(action, slot, margin, timer, host);
    return true;
  }
  if (!host.EffectsBusy()) AdvanceBattleAbilitySequence(host);
  return false;
}
}  // namespace

// decomp: BATTLE.PRG func_80069C6C (src/BATTLE/BATTLE.PRG/146C.c:735)
void ArmDefenseAbilityTimingWindow(int target_actor_slot, AbilityHost& host) {
  if (target_actor_slot != 0 || host.QueuedAction().primary_actor == 0) {
    return;
  }
  if (host.Field(AbilityField::kAbilityWindow) == 0 &&
      CanTriggerBattleAbility(0, host)) {
    host.SetField(
        AbilityField::kAbilityWindow,
        host.Field(AbilityField::kAbilityTimer) + host.TickSpeed() * 2);
  }
}

// decomp: BATTLE.PRG _isArtOrAbilityUnlocked
// (src/BATTLE/BATTLE.PRG/146C.c:3270)
PendingAbilityUnlocks PendingBattleAbilityUnlocks(const AbilityHost& host) {
  PendingAbilityUnlocks pending;
  const int category = host.EquippedWeaponCategory() % kWeaponCategories;
  const int learned = host.ArtsLearned(category);
  if (learned < kBreakArtTiers) {
    pending.break_art = host.WeaponCategoryKills(category) >=
                        host.BreakArtRequirement(category, learned);
  }
  if (!pending.break_art) {
    const int unlocked = host.BattleAbilitiesUnlocked();
    pending.battle_ability =
        unlocked < kBattleAbilityUnlocks &&
        host.TotalKills() >= host.BattleAbilityRequirement(unlocked);
  }
  return pending;
}

// decomp: BATTLE.PRG func_8006F89C (src/BATTLE/BATTLE.PRG/146C.c:3312)
void BeginPendingAbilityReward(AbilityHost& host) {
  const int mode = host.RecoveryRewardMode();
  if (mode == kRewardWait) {
    host.WaitForPostActionMovement();
    return;
  }
  const bool earned =
      mode == kRewardCheckUnlocks && PendingBattleAbilityUnlocks(host).Any();
  if (earned || IsNewActionLearned(host)) {
    BeginAbilityLearnedPose(host);
  } else {
    host.WaitForPostActionMovement();
  }
}

// decomp: BATTLE.PRG func_8006FA20 (src/BATTLE/BATTLE.PRG/146C.c:3352)
void GrantPendingAbilityUnlock(AbilityHost& host) {
  if (IsNewActionLearned(host)) {
    const int action_id = host.Selection().action_id;
    host.SetActionFlowMode(kFlowAbilityUnlockMenu);
    host.PauseFieldForMenu();
    host.ShowActionLearnedNotification(action_id);
    host.SetActionDefinitionUnlocked(action_id);
    return;
  }
  const PendingAbilityUnlocks pending = PendingBattleAbilityUnlocks(host);
  host.SetActionFlowMode(kFlowAbilityUnlockMenu);
  host.PauseFieldForMenu();
  host.OpenAbilityUnlockMenu(pending);
}

// decomp: BATTLE.PRG func_8006FD1C (src/BATTLE/BATTLE.PRG/146C.c:3420)
void AdvanceBattleAbilitySequence(AbilityHost& host) {
  host.SetTickSpeed(kChainTickSpeed);
  const ActionRequestView action = host.QueuedAction();
  if (action.action_id >= kFirstSpellSequenceAction &&
      action.action_id < kFirstBreakArtAction) {
    host.ReleaseActorSequence(action.primary_actor, kSpellSequenceKind);
  } else if (action.action_id >= kFirstBreakArtAction &&
             action.action_id < kEndBreakArtAction) {
    host.ReleaseActorSequence(action.primary_actor, kBreakArtSequenceKind);
  }
  if (host.QueueIndex() >= kMaxChainSteps) {
    host.FinishBattleAbilitySequence();
    return;
  }
  const int follow_up = host.Field(AbilityField::kAbilityInputSucceeded);
  if (follow_up == kChainFollowUp) {
    ContinueChain(action, host);
  } else if (follow_up == kDefenseFollowUp) {
    ContinueDefense(host);
  } else {
    host.FinishBattleAbilitySequence();
  }
}

// decomp: BATTLE.PRG func_80078828 (src/BATTLE/BATTLE.PRG/146C.c:7043)
bool CanTriggerBattleAbility(int ability_input_slot, const AbilityHost& host) {
  const ActionRequestView action = host.QueuedAction();
  if (action.primary_targets_position || action.target_count == 0 ||
      action.first_target_targets_position) {
    return false;
  }
  const AbilityKind kind = KindFor(action);
  bool result = true;
  if (ability_input_slot == 0 && !HasMappedAbility(kind, host)) {
    result = false;
  }
  if (action.first_target_actor != 0 &&
      host.ActorRoomSpawnIndex(action.first_target_actor) ==
          kDetachedSpawnIndex) {
    result = false;
  }
  if (action.primary_actor != 0) {
    if (action.first_target_actor != 0) result = false;
  } else if (action.action_id >= kFirstAbilityAction) {
    result = false;
  }
  if (static_cast<unsigned>(action.action_id - kFirstAbilityAction) <
      kAbilityActionCount) {
    result = false;
  }
  if (action.primary_actor == action.first_target_actor) result = false;
  if (ability_input_slot != 0) {
    if (!ValidAbilitySlot(ability_input_slot)) {
      VS_LOG_WARN("battle.ability", "invalid ability input slot={}",
                  ability_input_slot);
      return false;
    }
    if (!host.ActionAvailable(
            host.MappedAbility(kind, ability_input_slot - 1))) {
      result = false;
    }
    if (host.Field(AbilityField::kPreviousChainInput) == ability_input_slot) {
      result = false;
    }
  }
  if (host.Field(AbilityField::kAbilityInputAttempted) != 0) result = false;
  return result;
}

// decomp: BATTLE.PRG func_80078AB4 (src/BATTLE/BATTLE.PRG/146C.c:7147)
void UpdateBattleAbilityTiming(AbilityHost& host) {
  const ActionRequestView action = host.QueuedAction();
  const int margin = AbilityTimingMargin(action, host);
  const int timer = host.Field(AbilityField::kAbilityTimer);
  const int window = host.Field(AbilityField::kAbilityWindow);
  // The original divides with no zero check; a zero tick speed gives the
  // R3000 quotient instead of trapping.
  if (window != 0 && timer == window - host.TickSpeed() * 2 &&
      CanTriggerBattleAbility(0, host)) {
    host.ShowAbilityTimingCue(
        R3000Quotient(margin * kTimingCueMultiplier, host.TickSpeed()));
  }
  host.SetField(AbilityField::kAbilityTimer,
                host.Field(AbilityField::kAbilityTimer) + host.TickSpeed());
  if ((host.PressedButtons() & kAbilityButtons) != 0 &&
      !HandleAbilityInput(action, margin, timer, host)) {
    return;
  }
  if (!action.primary_targets_position) {
    if (host.IsActorActionAnimationBusy(action.primary_actor)) return;
    if (host.Field(AbilityField::kAbilityInputAttempted) != 0 ||
        host.Field(AbilityField::kChainAbilitySlot) == 0) {
      host.RestoreActorIdlePose(action.primary_actor);
    }
    if (!action.first_target_targets_position &&
        host.ActorHp(action.first_target_actor) == 0) {
      host.RestoreActorIdlePose(action.primary_actor);
    }
  }
  if (!host.EffectsBusy()) AdvanceBattleAbilitySequence(host);
}

// decomp: BATTLE.PRG vs_battle_isActionUnlocked
// (src/BATTLE/BATTLE.PRG/146C.c:9241)
bool IsActionUnlocked(std::uint32_t action_id, const AbilityHost& host) {
  return action_id - 1 < kLastUnlockableAction &&
         host.ActionDefinitionUnlocked(static_cast<int>(action_id));
}

// decomp: BATTLE.PRG vs_battle_setActionUnlocked
// (src/BATTLE/BATTLE.PRG/146C.c:9249)
void UnlockAction(std::uint32_t action_id, AbilityHost& host) {
  if (action_id - 1 < kLastUnlockableAction) {
    host.SetActionDefinitionUnlocked(static_cast<int>(action_id));
  }
}

// decomp: BATTLE.PRG vs_battle_isSpellClassUnlocked
// (src/BATTLE/BATTLE.PRG/5BF94.c:3631)
bool HasUnlockedSpellClass(int spell_class, const AbilityHost& host) {
  const int count = host.SpellClassCount(spell_class);
  for (int index = 0; index < count; ++index) {
    if (host.ActionDefinitionUnlocked(
            host.SpellClassSpellId(spell_class, index))) {
      return true;
    }
  }
  return false;
}

// decomp: BATTLE.PRG _breakArtsUnlocked (src/BATTLE/BATTLE.PRG/5BF94.c:3643)
bool HasUnlockedWeaponBreakArt(const AbilityHost& host) {
  const int first = kFirstBreakArtAction +
                    (host.EquippedWeaponCategory() - 1) * kBreakArtsPerCategory;
  for (int tier = 0; tier < kBreakArtsPerCategory; ++tier) {
    if (host.ActionDefinitionUnlocked(first + tier)) return true;
  }
  return false;
}

// decomp: BATTLE.PRG vs_battle_chainAbilitiesUnlocked
// (src/BATTLE/BATTLE.PRG/5BF94.c:3657)
bool HasUnlockedBattleAbility(int defense, const AbilityHost& host) {
  const AbilityKind kind =
      defense == 0 ? AbilityKind::kChain : AbilityKind::kDefense;
  for (unsigned index = 0; index < kAbilityActionCount; ++index) {
    if (host.ActionDefinitionUnlocked(
            host.AbilityActionId(kind, static_cast<int>(index)))) {
      return true;
    }
  }
  return false;
}
}  // namespace VS::Battle
