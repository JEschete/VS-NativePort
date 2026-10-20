// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c, battle_movement.c and
// battle_gte.c, revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/DecisionPrimitives.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <span>
#include <utility>

#include "Battle/Ai/PlanningSort.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Damage.h"
#include "Field/Gimmicks.h"
}

namespace {
constexpr int kActorSlots = 16;
constexpr unsigned kLimbCount = 6;
constexpr unsigned kActionsPerLimb = 4;
constexpr unsigned kHostileRelation = 3;
constexpr int kSpecialTargetAction = 0x60;
constexpr unsigned kSpecialTargetClass = 2;
constexpr unsigned kHarmfulBit = 15;
constexpr unsigned kBeneficialBit = 14;
constexpr unsigned kNearbyDistanceSquared = 3;
constexpr int kGimmickRangeSquared = 0x19000;
constexpr unsigned kRelationReady = 0x80000000;
constexpr unsigned kRelationVisibility = 1u << 2;
constexpr unsigned kPromotionRollMask = 0x7F;
constexpr unsigned kMaximumPromotionWeight = 0x4C;
constexpr int kStatusWeight = 6;
constexpr int kHpWeight = 4;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("ai", "%s", message);
    std::abort();
  }
}

void RequireActorId(int id) {
  Require(id >= 0 && id < kActorSlots, "AI decision actor ID is out of bounds");
}

int16_t SignedHalf(std::uint32_t value) {
  return std::bit_cast<int16_t>(static_cast<std::uint16_t>(value));
}

int TargetActionBit(int action_id, int actor_id, unsigned bit) {
  RequireActorId(actor_id);
  Require(action_id >= 0 && static_cast<std::size_t>(action_id) <
                                std::size(Battle_actionDefinitions),
          "AI decision action ID is out of bounds");
  const auto* actor = Battle_actors[actor_id];
  Require(actor != nullptr && actor->stats != nullptr,
          "AI target classification requires actor stats");
  const auto flags = std::bit_cast<std::uint16_t>(
                         Battle_actionDefinitions[action_id].aiFlags) >>
                     bit;
  if (action_id == kSpecialTargetAction &&
      actor->stats->enemyClass == kSpecialTargetClass) {
    return flags ^ 1;
  }
  return flags & 1;
}
}  // namespace

// decomp: func_800DBC60 (src/BATTLE/BATTLE.PRG/73460.s:7).
int Battle_Ai_DoesActionRequireCooldown(Battle_Ai_ActorAiState* state,
                                        Battle_Ai_ActionCandidate* candidate) {
  Require(state != nullptr && candidate != nullptr,
          "AI cooldown check requires state and candidate");
  return state->usesRoomActionDelay != 0 ||
         (candidate->opaque10.opaque0Bit4 != 0 &&
          Field_gimmickInstances[candidate->targetId] == nullptr);
}

// decomp: func_800DBCB4 (src/BATTLE/BATTLE.PRG/73460.s:38).
int Battle_Ai_IsActionCooldownReady(Battle_Ai_ActorAiState* state,
                                    Battle_Ai_ActionCandidate* candidate) {
  Require(state != nullptr && candidate != nullptr,
          "AI cooldown readiness requires state and candidate");
  return state->actionDelay == 0 &&
         (!Battle_Ai_DoesActionRequireCooldown(state, candidate) ||
          state->minActionDelay == 0);
}

// decomp: func_800DBCEC (src/BATTLE/BATTLE.PRG/73460.s:61).
// Legacy return convention: zero means a hostile actor is nearby.
int Battle_Ai_HasNearbyEnemy(Battle_Ai_ActorAiState* state, int target_id) {
  Require(state != nullptr, "AI proximity check requires state");
  RequireActorId(target_id);
  const auto* target = Field_gimmickInstances[target_id];
  Require(target != nullptr, "AI proximity check requires a target gimmick");
  // Walk actual native actors, not the assembly's extra pointer-array
  // pseudo-node.
  for (const auto* actor = Battle_actors[0]; actor != nullptr;
       actor = actor->next) {
    RequireActorId(actor->id);
    if (state->relations[actor->id].classification < kHostileRelation) continue;
    const auto* record = Actors_actorRecords[actor->id];
    Require(record != nullptr, "AI proximity check requires an actor model");
    const int dx = record->instance.currentTileX -
                   static_cast<std::uint8_t>(target->tileX);
    const int dz = record->instance.currentTileZ -
                   static_cast<std::uint8_t>(target->tileZ);
    if (static_cast<unsigned>(dx * dx + dz * dz) < kNearbyDistanceSquared) {
      return 0;
    }
  }
  return 1;
}

// decomp: func_800DBD80 (src/BATTLE/BATTLE.PRG/73580.s:11).
int Battle_Ai_CanExecuteActionCandidate(Battle_Ai_ActorAiState* state,
                                        Battle_Ai_ActionCandidate* candidate) {
  Require(state != nullptr && candidate != nullptr,
          "AI execution check requires state and candidate");
  if (candidate->weight == 0) return 0;
  Require(Battle_Ai_roomAiState != nullptr,
          "AI execution check requires room state");
  if (Battle_Ai_roomAiState->readinessLevel < candidate->threshold ||
      !Battle_Ai_IsActionCooldownReady(state, candidate)) {
    return 0;
  }
  const auto target = candidate->targetId;
  const auto distance =
      std::bit_cast<std::int32_t>(state->distancesSquared[target]);
  if (Field_gimmickInstances[target] != nullptr &&
      (distance > kGimmickRangeSquared || state->usesRoomActionDelay != 0 ||
       Battle_Ai_HasNearbyEnemy(state, static_cast<int>(target)))) {
    return 0;
  }
  if (candidate->distanceSquared < distance) return 0;
  const auto& options = candidate->opaque10;
  const auto range = options.opaque0Bit6;
  const auto range_squared = std::bit_cast<std::int32_t>(range * range);
  if (range != 0 && options.opaque0Bit1 != 0 && distance < range_squared) {
    return 0;
  }
  const auto relation = std::bit_cast<std::uint32_t>(state->relations[target]);
  if ((relation & kRelationReady) == 0) return 0;
  const auto* player = Actors_actorRecords[0];
  Require(player != nullptr, "AI execution check requires Ashley's model");
  if (player->instance.movementMode != 0 ||
      player->instance.locomotionState == 1) {
    return 0;
  }
  if (options.opaque0Bit2 == 0 && (relation & kRelationVisibility) == 0) {
    return 0;
  }
  return options.opaque0Bit3 != 0 || state->relations[target].blocked == 0;
}

// decomp: func_800DC284 (src/BATTLE/BATTLE.PRG/7399C.s:79).
// Native scoring is pure arithmetic; incidental GTE scratch clobbers are not an
// API.
int Battle_Ai_ScoreActorCondition(int status, int hp, int mp, int dp) {
  const auto score = std::int64_t{status} * kStatusWeight +
                     std::int64_t{SignedHalf(hp)} * kHpWeight + SignedHalf(mp) +
                     std::int64_t{SignedHalf(dp)} * 2;
  Require(score >= std::numeric_limits<int>::min() &&
              score <= std::numeric_limits<int>::max(),
          "AI condition score exceeds its signed priority range");
  return static_cast<int>(score);
}

// decomp: func_800DC484 (src/BATTLE/BATTLE.PRG/73C84.s:10).
int Battle_Ai_IsActionHarmfulToActor(int action_id, int actor_id) {
  return TargetActionBit(action_id, actor_id, kHarmfulBit);
}

// decomp: func_800DC48C (src/BATTLE/BATTLE.PRG/73C84.s:16).
int Battle_Ai_IsActionBeneficialToActor(int action_id, int actor_id) {
  return TargetActionBit(action_id, actor_id, kBeneficialBit);
}

// decomp: func_800DC4F0 (src/BATTLE/BATTLE.PRG/73C84.s:52).
void Battle_Ai_CacheFallbackActionCandidate(
    Battle_Ai_ActionCandidate* candidate, Battle_ActionDefinition* action) {
  Require(candidate != nullptr && action != nullptr,
          "AI fallback cache requires candidate and action");
  if (action->aiFlags.reserved0Bit6 == 0) return;
  Require(candidate->row < kLimbCount && candidate->column < kActionsPerLimb,
          "AI fallback action slot is out of bounds");
  const auto bit = 1u << (candidate->row * kActionsPerLimb + candidate->column);
  auto mask = static_cast<std::uint32_t>(Battle_Ai_unusedPlanCounter);
  if ((mask & bit) != 0) return;
  Require(Battle_Ai_fallbackCandidateCount >= 0 &&
              Battle_Ai_fallbackCandidateCount <= kActorSlots,
          "AI fallback cache count is out of bounds");
  Battle_Ai_unusedPlanCounter = static_cast<int>(mask | bit);
  if (Battle_Ai_fallbackCandidateCount == kActorSlots) return;
  Require(Battle_Ai_fallbackCandidates != nullptr,
          "AI fallback cache storage is not initialized");
  auto& destination =
      Battle_Ai_fallbackCandidates[Battle_Ai_fallbackCandidateCount];
  static_assert(sizeof(destination) == sizeof(*candidate));
  std::memmove(&destination, candidate, sizeof(destination));
  ++Battle_Ai_fallbackCandidateCount;
}

// decomp: func_800DC574 (src/BATTLE/BATTLE.PRG/73C84.s:89).
void Battle_Ai_PromoteRandomizedActionCandidate(Battle_Ai_ActionPlanView* state,
                                                int count) {
  Require(state != nullptr, "AI candidate promotion requires state");
  Require(count >= 0 && count <= state->candidateCount &&
              state->candidateCount <= kActorSlots,
          "AI candidate promotion count is out of bounds");
  auto entries =
      std::span{state->candidateOrder}.first(static_cast<std::size_t>(count));
  Require(std::ranges::all_of(
              entries, [](const auto* entry) { return entry != nullptr; }),
          "AI candidate promotion contains a null entry");
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto roll =
        static_cast<unsigned>(Core_Random_Rand()) & kPromotionRollMask;
    const auto* candidate = entries[index];
    if (candidate->action != 0 &&
        roll < std::min(candidate->weight, kMaximumPromotionWeight)) {
      std::swap(entries.front(), entries[index]);
      VS::Battle::ai::SortCandidatePointers(entries.subspan(1));
      return;
    }
  }
}

// decomp: func_800E45D4 (src/BATTLE/BATTLE.PRG/7BDD4.s:10).
int Battle_Ai_NextActorRandomMasked(int range) {
  Require(range > 0, "AI random mask requires a positive range");
  auto& frame = *Battle_Ai_FrameState();
  const auto next =
      std::int64_t{SignedHalf(frame.randomState)} + frame.lastBudgetTick;
  Require(next >= std::numeric_limits<int>::min() &&
              next <= std::numeric_limits<int>::max(),
          "AI actor random accumulator overflow");
  frame.randomState = static_cast<std::uint32_t>(next);
  return static_cast<int>(frame.randomState &
                          (static_cast<unsigned>(range) - 1));
}
