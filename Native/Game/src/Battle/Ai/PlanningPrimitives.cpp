// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c and battle_movement.c,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0; THIRD_PARTY.md).
#include "Battle/Ai/PlanningPrimitives.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>
#include <utility>

#include "Battle/Ai/PlanningSort.hpp"

extern "C" {
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
}

namespace {
constexpr int kLimbCount = 6;
constexpr int kActionsPerLimb = 4;
constexpr unsigned kRelationClassThree = 3;
constexpr unsigned kUnconditionalTargetFlag = 0x100;
constexpr int kTargetConditionShift = 9;
constexpr int kSpellBlockingStatus = 0x1000;
constexpr int kSpellsAndAbilitiesOnlyStatus = 0x2000;
constexpr int kResourceRestrictingStatuses = 0xE2B0;
constexpr unsigned kPpActionType = 4;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("ai", "%s", message);
    std::abort();
  }
}

const Battle_ActorStats& ActorStats(const Battle_Ai_ActorAiState* state) {
  Require(state != nullptr && state->actor != nullptr &&
              state->actor->opaque3C != nullptr,
          "AI planning requires an actor and its stats");
  return *state->actor->opaque3C;
}

void RequireSlot(int row, int column) {
  Require(
      row >= 0 && row < kLimbCount && column >= 0 && column < kActionsPerLimb,
      "AI planning action slot is out of bounds");
}
}  // namespace

namespace VS::Battle::ai {
// decomp: func_800E6974 / L800E6958 (src/BATTLE/BATTLE.PRG/7E05C.s:114).
// Equal scores must retain the original midpoint partition's tie ordering.
void SortCandidatePointers(std::span<Battle_Ai_CandidateEntryView*> entries) {
  if (entries.size() < 2) return;
  std::swap(entries.front(), entries[(entries.size() - 1) / 2]);
  std::size_t boundary = 0;
  for (std::size_t index = 1; index < entries.size(); ++index) {
    if (entries[index]->reserved0 > entries.front()->reserved0) {
      std::swap(entries[++boundary], entries[index]);
    }
  }
  std::swap(entries.front(), entries[boundary]);
  SortCandidatePointers(entries.first(boundary));
  SortCandidatePointers(entries.subspan(boundary + 1));
}
}  // namespace VS::Battle::ai

// decomp: func_800DC2E0 (src/BATTLE/BATTLE.PRG/73AE0.s).
void Battle_Ai_CopyActionCandidate(Battle_Ai_ActionCandidate* destination,
                                   Battle_Ai_ActionCandidate* source) {
  Require(destination != nullptr && source != nullptr,
          "AI candidate copy requires source and destination");
  // The assembly reads all five words before writing any of them.
  std::memmove(destination, source, sizeof(*destination));
}

// decomp: func_800DC30C (src/BATTLE/BATTLE.PRG/73B0C.s).
void Battle_Ai_ApplySimulatedStatDelta(int16_t* value, int16_t* maximum,
                                       int delta, int operation) {
  Require(value != nullptr && maximum != nullptr && *maximum >= 0,
          "AI simulated stat delta requires a nonnegative maximum");
  const auto current = static_cast<std::int64_t>(*value);
  const auto next = operation == 2 ? current + delta : current - delta;
  *value = static_cast<int16_t>(
      std::clamp(next, std::int64_t{0}, std::int64_t{*maximum}));
}

// decomp: func_800DC344 (src/BATTLE/BATTLE.PRG/73B0C.s).
void Battle_Ai_AppendActionCandidate(Battle_Ai_ActorAiState* state,
                                     Battle_Ai_ActionCandidate* candidate) {
  Require(state != nullptr && candidate != nullptr,
          "AI candidate append requires state and candidate");
  auto candidates = std::span{state->candidates};
  Require(state->candidateCount >= 0 &&
              state->candidateCount <= static_cast<int>(candidates.size()),
          "AI candidate count is out of bounds");
  if (state->candidateCount == static_cast<int>(candidates.size()) ||
      candidate->weight == 0) {
    return;
  }
  Require(Battle_Ai_candidateExclusions != nullptr,
          "AI candidate exclusion table is not initialized");
  const auto index = static_cast<std::size_t>(state->candidateCount);
  Battle_Ai_candidateExclusions[candidate->targetId][candidate->action] = 1;
  Battle_Ai_CopyActionCandidate(&candidates[index], candidate);
  state->candidateOrder[index] = &candidates[index];
  ++state->candidateCount;
}

// decomp: func_800DC3CC (src/BATTLE/BATTLE.PRG/73B0C.s);
// partition: func_800E6974 / L800E6958 (src/BATTLE/BATTLE.PRG/7E05C.s).
void Battle_Ai_SortActionCandidates(Battle_Ai_ActionPlanView* state) {
  Require(state != nullptr, "AI candidate sort requires state");
  std::span<Battle_Ai_CandidateEntryView*> order{state->candidateOrder};
  Require(state->candidateCount >= 0 &&
              state->candidateCount <= static_cast<int>(order.size()),
          "AI candidate sort count is out of bounds");
  order = order.first(static_cast<std::size_t>(state->candidateCount));
  Require(std::ranges::all_of(
              order, [](const auto* entry) { return entry != nullptr; }),
          "AI candidate sort contains a null entry");
  VS::Battle::ai::SortCandidatePointers(order);
}

// decomp: func_800DC3E8 (src/BATTLE/BATTLE.PRG/73B0C.s).
void Battle_Ai_CopyPlanningActorStats(Battle_ActorStats* source,
                                      Battle_ActorStats* destination) {
  Require(source != nullptr && destination != nullptr,
          "AI stat snapshot requires source and destination");
  if (source == destination) return;
  destination->currentHp = source->currentHp;
  destination->maxHp = source->maxHp;
  destination->currentMp = source->currentMp;
  destination->maxMp = source->maxMp;
  destination->movementClass = source->movementClass;
  destination->walkSpeed = source->walkSpeed;
  destination->gaitClass = source->gaitClass;
  destination->runSpeed = source->runSpeed;
  destination->statuses = source->statuses;
  std::copy_n(source->limbs, std::size(source->limbs), destination->limbs);
}

// decomp: func_800E4CF4 (src/BATTLE/BATTLE.PRG/7C4F4.s).
int Battle_Ai_CanAiActionAffectActor(Battle_Ai_ActorAiState* state,
                                     Battle_Ai_ActionCandidate* candidate,
                                     int actor_id) {
  Require(state != nullptr && candidate != nullptr,
          "AI target eligibility requires state and candidate");
  Require(actor_id >= 0 &&
              actor_id < static_cast<int>(std::size(Battle_Ai_actorAiStates)),
          "AI target actor ID is out of bounds");
  RequireSlot(candidate->row, candidate->column);
  const auto* peer = Battle_Ai_actorAiStates[actor_id];
  const auto relation = state->relations[actor_id].classification;
  if (peer == nullptr || relation < 2) return 0;
  if (candidate->opaque10.opaque0Bit5 != 0) {
    if (relation >= kRelationClassThree) return 0;
    const auto conditions = (peer->resourceStatusRestricted & 1u) |
                            ((peer->lowMp & 1u) << 1) |
                            ((peer->lowHp & 1u) << 2);
    const auto action_flags = std::bit_cast<std::uint16_t>(
        Battle_actionDefinitions[candidate->action].aiFlags);
    if ((action_flags & kUnconditionalTargetFlag) == 0 &&
        ((action_flags >> kTargetConditionShift) & conditions) == 0) {
      return 0;
    }
  }
  if (candidate->opaque10.opaque0Bit4 != 0 && relation < kRelationClassThree) {
    return 0;
  }
  const auto& slot = state->actionSlots[candidate->row][candidate->column];
  return !(slot.opaque0Bit1 != 0 && actor_id == state->actorId);
}

// decomp: func_800E4DF8 (src/BATTLE/BATTLE.PRG/7C4F4.s).
int Battle_Ai_RefreshAiActionUsability(Battle_Ai_ActorAiState* state, int row,
                                       int column) {
  RequireSlot(row, column);
  const auto& stats = ActorStats(state);
  const auto action_id = stats.limbActions[row][column].actionId;
  Require(action_id < std::size(Battle_actionDefinitions),
          "AI action ID is out of bounds");
  const auto& action = Battle_actionDefinitions[action_id];
  bool usable = stats.limbs[row].hp >= 2 &&
                ((stats.statuses & kSpellsAndAbilitiesOnlyStatus) == 0 ||
                 action.type == Battle_ActionType_Spell ||
                 action.type == Battle_ActionType_Ability);
  if (usable) {
    switch (action.type) {
      case Battle_ActionType_Spell:
        usable = stats.currentMp >= action.cost &&
                 (stats.statuses & kSpellBlockingStatus) == 0;
        break;
      case Battle_ActionType_BreakArt:
        usable = static_cast<std::uint16_t>(stats.currentHp) >=
                 action.cost + static_cast<std::uint16_t>(stats.maxHp) / 2;
        break;
      case kPpActionType:
        usable =
            std::bit_cast<std::int16_t>(stats.weapon.currentPp) >= action.cost;
        break;
      default:
        break;
    }
  }
  state->actionSlots[row][column].opaque0Bit7 = usable;
  return usable;
}

// decomp: func_800E4F14 (src/BATTLE/BATTLE.PRG/7C4F4.s).
void Battle_Ai_RefreshAiResourceRestrictions(Battle_Ai_ActorAiState* state) {
  const auto& stats = ActorStats(state);
  const auto hp = static_cast<std::uint16_t>(stats.currentHp);
  const auto mp = static_cast<std::uint16_t>(stats.currentMp);
  const auto threshold = state->actionCostEnabled;
  state->lowHp = hp * 2 < static_cast<std::uint16_t>(stats.maxHp);
  state->lowMp =
      threshold != 0 &&
      (mp < threshold || mp * 2 < static_cast<std::uint16_t>(stats.maxMp));
  state->resourceStatusRestricted =
      (stats.statuses & kResourceRestrictingStatuses) != 0 ||
      (threshold != 0 && (stats.statuses & kSpellBlockingStatus) != 0);
}
