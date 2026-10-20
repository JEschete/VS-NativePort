// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c:464/473/476,
// battle_movement.c:602-650 and actor_cleanup.c:19, revision
// 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/Engagement.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include "Battle/Ai/EngagementState.hpp"

extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
}

namespace {
constexpr int kActorSlots = 16;
constexpr unsigned kReadyFrames = 0x20;
constexpr unsigned kLeaveCooldown = 0x30;
constexpr unsigned kRejoinFrames = 90;
constexpr int kResetMarker = 0x40;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("ai", "%s", message);
    std::abort();
  }
}

void RequireActorId(int id) {
  Require(id >= 0 && id < kActorSlots,
          "AI engagement actor ID is out of bounds");
}

Battle_Ai_RoomAiState& Room() {
  Require(Battle_Ai_roomAiState != nullptr,
          "AI engagement requires room state");
  return *Battle_Ai_roomAiState;
}
}  // namespace

namespace VS::Battle::ai {
std::uint16_t EngagedMask(const Battle_Ai_RoomAiState& room) {
  std::uint16_t mask = 0;
  std::memcpy(&mask, room.engaged_mask_bytes, sizeof(mask));
  return mask;
}

void StoreEngagedMask(Battle_Ai_RoomAiState& room, std::uint16_t mask) {
  std::memcpy(room.engaged_mask_bytes, &mask, sizeof(mask));
}
}  // namespace VS::Battle::ai

// decomp: func_800D820C (src/BATTLE/BATTLE.PRG/6FA0C.s:9).
void Battle_Ai_SetActorAwarenessIndicator(int actor_id, int value) {
  RequireActorId(actor_id);
  if (actor_id != 0) Battle_SetActorMarker(actor_id, value);
}

// decomp: func_800D821C (src/BATTLE/BATTLE.PRG/6FA0C.s:22).
void Battle_Ai_ResetActorAwarenessIndicator(
    Battle_Ai_PlanEvaluationView* state) {
  Require(state != nullptr, "AI marker reset requires state");
  if (state->actorId == 0) return;
  Battle_Ai_SetActorAwarenessIndicator(state->actorId, 0);
  Battle_Ai_SetActorAwarenessIndicator(state->actorId, kResetMarker);
}

// decomp: func_800D826C (src/BATTLE/BATTLE.PRG/6FA6C.s:10).
int Battle_Ai_IsEngagementReady(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI engagement readiness requires state");
  return state->opaque8E >= kReadyFrames;
}

// decomp: func_800D8280 (src/BATTLE/BATTLE.PRG/6FA6C.s:23).
void Battle_Ai_UpdateTemporaryAwarenessIndicator(
    Battle_Ai_PerceptionUpdateView* state) {
  Require(state != nullptr, "AI temporary marker requires state");
  if (state->opaque12A == 0) return;
  Require(state->actor != nullptr, "AI temporary marker requires an actor");
  --state->opaque12A;
  Battle_Ai_SetActorAwarenessIndicator(
      state->actor->id, state->opaque12A == 0 ? 0 : state->opaque138);
}

// decomp: func_800D836C (src/BATTLE/BATTLE.PRG/6FA6C.s:99).
int Battle_Ai_TryEngageActor(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI engagement request requires state");
  RequireActorId(state->actorId);
  if (state->opaque8E != 0) return 1;
  auto& room = Room();
  if (room.engagedCount > room.engagementLimit || state->opaque98 != 0 ||
      Battle_Ai_candidateEngagedThisUpdate != 0) {
    return 0;
  }
  Require(room.engagedCount < kActorSlots,
          "AI engagement count exceeds actor storage");
  state->engagementRequest = 0;
  VS::Battle::ai::StoreEngagedMask(
      room, static_cast<std::uint16_t>(VS::Battle::ai::EngagedMask(room) |
                                       (1u << state->actorId)));
  ++room.engagedCount;
  state->opaque8E = state->opaque9A != 0 ? kReadyFrames : 1;
  if (room.engagedHead == nullptr) room.engagedTail = state;
  state->nextEngaged = room.engagedHead;
  room.engagedHead = state;
  return 1;
}

// decomp: func_800D82CC (src/BATTLE/BATTLE.PRG/6FA6C.s:59).
void Battle_Ai_DisengageActor(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI disengagement requires state");
  RequireActorId(state->actorId);
  auto& room = Room();
  const auto mask = VS::Battle::ai::EngagedMask(room);
  const auto bit = 1u << state->actorId;
  if ((mask & bit) == 0) return;
  Require(room.engagedCount > 0, "AI engagement membership count underflow");
  Require(state->actor != nullptr && Battle_actors[0] != nullptr,
          "AI disengagement requires actor context");
  VS::Battle::ai::StoreEngagedMask(room,
                                   static_cast<std::uint16_t>(mask & ~bit));
  state->opaque8E = 0;
  const auto* second = Battle_actors[0]->next;
  state->opaque98 =
      second != nullptr && second->next == nullptr ? 0 : kLeaveCooldown;
  state->opaque9A = kRejoinFrames;
  Battle_Ai_UnlinkEngagedActor(state);
  --room.engagedCount;
  Battle_Ai_SetActorAwarenessIndicator(state->actorId, 0);
}

// decomp: func_800D82A8 (src/BATTLE/BATTLE.PRG/6FA6C.s:42).
void Battle_Ai_DisengageActorById(int actor_id) {
  RequireActorId(actor_id);
  if (actor_id == 0 || Battle_Ai_actorAiStates[actor_id] == nullptr) return;
  Battle_Ai_DisengageActor(Battle_Ai_actorAiStates[actor_id]);
}
