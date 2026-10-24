// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/actor_cleanup.c:12-83,
// actor_update.c:44 and battle_movement.c:1688, revision
// 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/ActorCleanup.h"

#include <cstdint>
#include <cstdlib>
#include <iterator>

#include "Battle/Ai/EngagementState.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Memory/Heap.h"
}

namespace {
constexpr int kActorSlots = 16;
constexpr unsigned kReservationBit = 0x80;
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;
constexpr unsigned kShiftMask = 31;
constexpr unsigned kAttributeKindFlags = 0x14;
constexpr unsigned kAiKindFlags = 5;
constexpr unsigned kCommandModeMask = 0xFC;
constexpr unsigned kOrbitTypeFirst = 0xAC;
constexpr unsigned kOrbitTypeSecond = 0xAD;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("ai", "%s", message);
    std::abort();
  }
}

void RequireActorId(int id) {
  Require(id >= 0 && id < kActorSlots, "AI cleanup actor ID is out of bounds");
}

void ClearLinkedActorReferences(int id) {
  for (auto* peer : Battle_Ai_actorAiStates) {
    if (peer != nullptr && peer->linkedActor == id) {
      peer->linkedActor = kActorSlots;
    }
  }
}

void DetachActorReferences(Battle_Actor* actor) {
  const int id = actor->id;
  if ((id > 0 && id < kActorSlots) ||
      (actor->kind & kAttributeKindFlags) != 0) {
    RequireActorId(id);
    Battle_Ai_attributeActorMask = static_cast<int>(
        static_cast<unsigned>(Battle_Ai_attributeActorMask) & ~(1u << id));
  }
  ClearLinkedActorReferences(id);
  Require(Battle_Ai_roomAiState != nullptr, "AI cleanup requires room state");
  for (auto*& special : Battle_Ai_roomAiState->specialActors) {
    if (special == actor) special = nullptr;
  }
}

void ReleaseOrbitStorage(const Battle_Actor& actor) {
  if (actor.subType != kOrbitTypeFirst && actor.subType != kOrbitTypeSecond) {
    return;
  }
  auto* orbit = Battle_Ai_arenaOrbit;
  Battle_Ai_arenaOrbit = nullptr;
  if (orbit != nullptr) Core_Memory_FreeFromLowEnd(orbit);
}
}  // namespace

// decomp: func_800E48A8 (src/BATTLE/BATTLE.PRG/7BDD4.s:246).
void Battle_Ai_SetActorAiReactionWithDelay(Battle_Ai_PerceptionUpdateView* view,
                                           int value, int range) {
  Require(view != nullptr, "AI reaction request requires state");
  const int roll = Battle_Ai_NextActorRandomMasked(range);
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  if (state.nativePointer18C != nullptr || state.decisionDelay != 0 ||
      roll >= range) {
    return;
  }
  RequireActorId(state.actorId);
  auto* actor = Battle_actors[state.actorId];
  Require(actor != nullptr, "AI reaction request requires an actor");
  constexpr u_char kReactionDelay = 30;
  state.decisionDelay = kReactionDelay;
  actor->commandFlags = static_cast<u_char>(value);
}

// decomp: func_800E6700 (src/BATTLE/BATTLE.PRG/7DE94.s:42).
void Battle_Ai_RefreshActorAiReservations(int actor_id) {
  RequireActorId(actor_id);
  const auto* actor = Battle_actors[actor_id];
  constexpr unsigned kReleaseCommands = 0xA41800;
  if (actor == nullptr ||
      ((1u << (actor->commandOpcode & kShiftMask)) & kReleaseCommands) == 0) {
    return;
  }
  const auto* model = Actors_actorRecords[actor_id];
  Require(model != nullptr, "AI reservation refresh requires a model");
  constexpr unsigned kReactionFlags = 0xB;
  if (model->instance.movementMode != 0 ||
      model->instance.locomotionState != 0 ||
      (model->instance.reactionState & kReactionFlags) != 0) {
    return;
  }
  auto* state = Battle_Ai_actorAiStates[actor_id];
  if (state != nullptr) Battle_Ai_ReleaseActorAiReservations(state);
}

// decomp: func_800E7608 (src/BATTLE/BATTLE.PRG/7E05C.s:1052).
void Battle_Ai_ClearActorAiLinksAndPlan(int actor_id) {
  RequireActorId(actor_id);
  ClearLinkedActorReferences(actor_id);
  auto* state = Battle_Ai_actorAiStates[actor_id];
  if (state != nullptr) Battle_Ai_DisengageActor(state);
}

// decomp: func_800E4C28 (src/BATTLE/BATTLE.PRG/7BDD4.s:550).
void Battle_Ai_ReleaseAiDestinationTile(int x, int z) {
  if (Battle_Ai_tileFlagMap == nullptr) return;
  Require(x >= 0 && z >= 0 && x < kAiFrameGridSide && z < kAiFrameGridSide,
          "AI reservation release tile is out of bounds");
  Battle_Ai_tileFlagMap[z][x] &= static_cast<u_short>(~kReservationBit);
}

// decomp: func_800E5240 (src/BATTLE/BATTLE.PRG/7C7E0.s:196).
void Battle_Ai_FreeActorNavigationOverride(int actor_id) {
  RequireActorId(actor_id);
  auto* state = Battle_Ai_actorAiStates[actor_id];
  if (Battle_Ai_tileFlagMap == nullptr || state == nullptr) return;
  auto* route = state->nativePointer18C;
  state->nativePointer18C = nullptr;
  if (route != nullptr) Core_Memory_FreeFromHighEnd(route);
}

// decomp: func_800E678C (src/BATTLE/BATTLE.PRG/7DE94.s:91).
void Battle_Ai_ReleaseActorAiReservations(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr && Battle_Ai_roomAiState != nullptr,
          "AI reservation release requires actor and room state");
  state->opaque1A = 0;
  const auto& room = *Battle_Ai_roomAiState;
  const int x = static_cast<int>(state->nextTile.word & kTileMask);
  const int z =
      static_cast<int>((state->nextTile.word >> kTileZShift) & kTileMask);
  // Legacy room names at bytes 1/2 are X-min/Z-max in the original bounds word.
  if (x >= room.maxTileZ && x <= room.maxTileX && z >= room.minTileZ &&
      z <= room.minTileX) {
    Battle_Ai_ReleaseAiDestinationTile(x, z);
  }
  if (state->reserved19B[0] != 0) {
    auto& gates = Battle_Ai_roomAiState->reservedEPrefix[0];
    const auto value = static_cast<unsigned>(gates);
    gates = static_cast<u_char>(value & (0u - (1u << (value & kShiftMask))));
  }
}

// decomp: func_800E6764 (src/BATTLE/BATTLE.PRG/7DE94.s:73).
void Battle_Ai_ForceReleaseActorAiReservations(int actor_id) {
  RequireActorId(actor_id);
  auto* state = Battle_Ai_actorAiStates[actor_id];
  if (state == nullptr) return;
  Battle_Ai_ReleaseActorAiReservations(state);
  state->opaque1A = 1;
}

// decomp: func_800E7454 (src/BATTLE/BATTLE.PRG/7E05C.s:924).
void Battle_Ai_FreeActorAiState(Battle_Actor* actor) {
  Require(actor != nullptr, "AI teardown requires an actor");
  if (Battle_Ai_tileFlagMap == nullptr) return;
  Require(actor->id >= 0 &&
              static_cast<std::size_t>(actor->id) < std::size(Battle_actors),
          "AI teardown actor is out of bounds");
  DetachActorReferences(actor);
  // Script classification may change without retiring the owned AI allocation.
  auto* state =
      actor->id < kActorSlots ? Battle_Ai_actorAiStates[actor->id] : nullptr;
  if (state == nullptr && (actor->kind & kAiKindFlags) == 0) return;
  RequireActorId(actor->id);
  Battle_Ai_FreeActorNavigationOverride(actor->id);
  if (state != nullptr) Battle_Ai_ReleaseActorAiReservations(state);
  if (actor->id == 0) return;
  auto& room = *Battle_Ai_roomAiState;
  const auto mask = VS::Battle::ai::EngagedMask(room);
  const auto bit = 1u << actor->id;
  if ((mask & bit) != 0) {
    Require(room.engagedCount > 0 && state != nullptr,
            "AI teardown found inconsistent engagement membership");
    VS::Battle::ai::StoreEngagedMask(room,
                                     static_cast<std::uint16_t>(mask & ~bit));
    --room.engagedCount;
  }
  actor->commandFlags &= kCommandModeMask;
  if (state != nullptr) {
    Battle_Ai_ResetActorAwarenessIndicator(
        reinterpret_cast<Battle_Ai_PlanEvaluationView*>(state));
    Battle_Ai_UnlinkEngagedActor(state);
  }
  Battle_Ai_actorAiStates[actor->id] = nullptr;
  Battle_Ai_FrameState()->actors[actor->id] = nullptr;
  if (state != nullptr) Core_Memory_FreeFromLowEnd(state);
  ReleaseOrbitStorage(*actor);
}
