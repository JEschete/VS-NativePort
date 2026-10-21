// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:23-85,386-425,1513-1553,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/MovementBehavior.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/DecisionCallbacks.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/GroundNavigation.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Gimmicks.h"
#include "Field/TerrainHeight.h"
}

namespace {
enum class MovementMode {
  kIdle,
  kPursuit,
  kDestination,
  kWander,
  kFlee,
  kPatrol,
  kFacing,
  kCount
};

constexpr int kPursuitResult = 3;
constexpr int kWanderResult = 6;
constexpr int kModeCounterStride = 2;
constexpr unsigned kResetNavigationBits = 0xFE1;
constexpr unsigned kEnabledNavigationBits = 0x3C0;
constexpr unsigned kPatrolRows = 4;
constexpr int kWaypointsPerRow = 8;
constexpr unsigned kEnabledWaypoint = 0x8000;
constexpr unsigned kWaypointCoordinateMask = 31;
constexpr unsigned kWaypointZShift = 5;
constexpr unsigned kTargetSubtileShift = 6;
constexpr unsigned kBusyMovementMask = 0x38;
constexpr unsigned kPlatformLayerBase = 2;
constexpr unsigned kPlatformCount = 4;
constexpr unsigned kTileLayerShift = 8;
constexpr unsigned kTileZShift = 16;
constexpr unsigned kTilePaddingShift = 24;
constexpr unsigned kHostileClassification = 3;
constexpr int kRelationshipClasses = 4;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("ai", "%s", message);
    std::abort();
  }
}

void ResetNavigationFlags(Battle_Ai_ActorAiState& state) {
  state.navigationFlags &= ~kResetNavigationBits;
  if (state.movementEnabled != 0) {
    state.navigationFlags |= kEnabledNavigationBits;
  }
}

void SelectPatrolWaypoint(Battle_Ai_ActorAiState& state) {
  Require(Battle_Ai_patrolRoutes != nullptr && state.patrolRow < kPatrolRows,
          "AI patrol requires a valid four-row waypoint table");
  unsigned nearest = std::numeric_limits<unsigned>::max();
  for (int index = 0; index < kWaypointsPerRow; ++index) {
    const auto entry = Battle_Ai_patrolRoutes[state.patrolRow][index];
    if ((entry & kEnabledWaypoint) == 0) continue;
    const int x = static_cast<int>(entry & kWaypointCoordinateMask);
    const int z =
        static_cast<int>((entry >> kWaypointZShift) & kWaypointCoordinateMask);
    const auto distance =
        static_cast<unsigned>(std::abs(x - state.tile.tile.tileX) +
                              std::abs(z - state.tile.tile.tileZ));
    if (distance < nearest) {
      nearest = distance;
      state.patrolIndex = static_cast<u_char>(index);
    }
  }
  state.patrolCooldown = 0;
}

bool HasPlannedTile(const Actors_ModelInstance& model) {
  return model.movementMode != 0 || model.locomotionState != 0;
}

Battle_Ai_RouteGateTile PlatformGate(unsigned layer) {
  Require(layer >= kPlatformLayerBase &&
              layer - kPlatformLayerBase < kPlatformCount,
          "AI target has an invalid moving-platform layer");
  const auto group = static_cast<int>(layer - kPlatformLayerBase);
  const auto* waypoint =
      static_cast<const VECTOR*>(Field_MovingPlatformTargetWaypoint(group));
  Require(waypoint != nullptr && Battle_Ai_routeGates != nullptr,
          "AI platform targeting requires waypoint and route-gate data");
  Battle_Ai_RouteGateTile gate{};
  Battle_Ai_FindNearestRouteGateEntry(group, waypoint->vx, waypoint->vz, &gate);
  return gate;
}

Battle_Ai_TilePosition TargetTile(const Actors_ModelInstance& model,
                                  bool planned) {
  Battle_Ai_TilePosition tile{};
  tile.word =
      planned
          ? static_cast<unsigned>(model.previousTileX) |
                (static_cast<unsigned>(model.previousTileLayer)
                 << kTileLayerShift) |
                (static_cast<unsigned>(model.previousTileZ) << kTileZShift) |
                (static_cast<unsigned>(model.reserved63) << kTilePaddingShift)
          : static_cast<unsigned>(model.currentTileX) |
                (static_cast<unsigned>(model.currentTileLayer)
                 << kTileLayerShift) |
                (static_cast<unsigned>(model.currentTileZ) << kTileZShift) |
                (static_cast<unsigned>(model.reserved5F) << kTilePaddingShift);
  return tile;
}

Actors_ActorRecord& TargetModel(unsigned target) {
  Require(target < kAiFrameActorSlots && Actors_actorRecords[target] != nullptr,
          "AI pursuit requires an existing target model");
  return *Actors_actorRecords[target];
}
}  // namespace

// decomp: func_800E4BB8 (src/BATTLE/BATTLE.PRG/7BDD4.s:497).
void Battle_Ai_RefreshAiMovementCooldown(Battle_Ai_TerrainMoveView* view) {
  Require(view != nullptr, "AI movement cooldown requires state");
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  ResetNavigationFlags(state);
}

// decomp: func_800DB4AC (src/BATTLE/BATTLE.PRG/72B70.s:102).
int Battle_Ai_SetMovementBehavior(Battle_Ai_ActorAiState* state, int mode) {
  Require(state != nullptr && mode >= 0 &&
              mode < static_cast<int>(MovementMode::kCount),
          "AI movement requires state and a valid behavior mode");
  state->requestedMovementMode = static_cast<u_char>(mode);
  if (state->currentMovementMode == mode) return mode;
  int result = -1;
  switch (static_cast<MovementMode>(mode)) {
    case MovementMode::kPursuit: {
      auto& approach = *reinterpret_cast<Battle_Ai_TargetApproachView*>(state);
      approach.opaqueE4 = 0;
      approach.targetId = 0;
      approach.timeout = 0;
      approach.opaqueE7 = 0;
      approach.opaqueE8 = 0;
      approach.rangeMode = 0;
      result = kPursuitResult;
      break;
    }
    case MovementMode::kWander:
      state->wanderPhase = 0;
      state->wanderCooldown = 0;
      result = kWanderResult;
      break;
    case MovementMode::kPatrol:
      SelectPatrolWaypoint(*state);
      result = 1;
      break;
    case MovementMode::kFacing:
      state->previousFacingTile.word = -1;
      state->facingPending = 0;
      break;
    default:
      break;
  }
  state->currentMovementMode = static_cast<u_char>(mode);
  state->movementModeCounters[static_cast<std::size_t>(mode) *
                              kModeCounterStride] = 0;
  ResetNavigationFlags(*state);
  return result;
}

// decomp: func_800E4904 (src/BATTLE/BATTLE.PRG/7BDD4.s:279).
void Battle_Ai_RefreshAiTargetTile(void* view) {
  Require(view != nullptr, "AI target refresh requires state");
  auto& state = *static_cast<Battle_Ai_TargetApproachView*>(view);
  const auto target = state.targetId;
  Require(target < kAiFrameActorSlots, "AI target ID is out of bounds");
  if (Battle_Ai_FrameState()->actors[target] == nullptr) return;
  const auto& model = TargetModel(target).instance;
  const bool planned = HasPlannedTile(model);
  const auto layer = planned ? model.previousTileLayer : model.currentTileLayer;
  if ((!planned && layer >= kPlatformLayerBase) || (planned && layer != 0)) {
    const auto gate = PlatformGate(layer);
    state.targetTileX = static_cast<u_char>(gate.opaque0 * 2);
    state.targetTileZ = static_cast<u_char>(gate.opaque2 * 2);
  } else {
    const int planned_x = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(model.supportOffset));
    state.targetTileX = static_cast<u_char>(
        (planned ? planned_x : model.position.vx) >> kTargetSubtileShift);
    state.targetTileZ =
        static_cast<u_char>((planned ? model.reserved50 : model.position.vz) >>
                            kTargetSubtileShift);
  }
}

// decomp: func_800E48F8 (src/BATTLE/BATTLE.PRG/7BDD4.s:270).
void Battle_Ai_RefreshAiTargetTileWhenTracking(
    Battle_Ai_PerceptionUpdateView* view) {
  Require(view != nullptr, "AI conditional target refresh requires state");
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  if (state.requestedMovementMode == static_cast<int>(MovementMode::kPursuit)) {
    Battle_Ai_RefreshAiTargetTile(view);
  }
}

// decomp: func_800DB370 (src/BATTLE/BATTLE.PRG/72B70.s:11).
int Battle_Ai_ApproachActor(Battle_Ai_ActorAiState* state, int target_id,
                            int distance, int option) {
  Require(state != nullptr && target_id >= 0 && target_id < kAiFrameActorSlots,
          "AI approach requires state and a valid target ID");
  if (Battle_Ai_FrameState()->actors[target_id] == nullptr) {
    return target_id != 0 ? Battle_Ai_SetMovementBehavior(
                                state, static_cast<int>(MovementMode::kPursuit))
                          : 0;
  }

  auto& approach = *reinterpret_cast<Battle_Ai_TargetApproachView*>(state);
  const auto& model = TargetModel(static_cast<unsigned>(target_id)).instance;
  if (approach.targetId != target_id) {
    approach.opaqueE4 = approach.timeout = approach.opaqueE7 = 0;
    approach.opaqueE8 = approach.rangeMode = approach.previousOption = 0;
    approach.previousTargetId = 0;
  }
  approach.targetId = static_cast<u_char>(target_id);
  approach.range = distance;
  const bool busy = (state->movementFlags & kBusyMovementMask) != 0;
  const bool planned = !busy && HasPlannedTile(model);
  const auto layer =
      planned ? model.previousTileLayer
              : (state->settingsBit14 != 0 || busy ? model.supportLayer : 0);
  if (planned || layer == 0) {
    state->destination = TargetTile(model, planned);
  }
  if (layer != 0) {
    const auto gate = PlatformGate(layer);
    state->destination.tile.tileX = gate.opaque0;
    state->destination.tile.tileZ = gate.opaque2;
    approach.cachedDestination.word = state->destination.word;
    // The original first writes an OR-combined word, not a packed X/Z pair.
    approach.targetTileX =
        static_cast<u_char>((gate.opaque0 | gate.opaque2) * 2);
    approach.reservedF5[0] = 0;
    approach.targetTileZ = 0;
    approach.reservedF7[0] = 0;
  }
  return Battle_Ai_UpdateTargetApproachMovement(&approach, option) != 0
             ? Battle_Ai_SetMovementBehavior(
                   state, static_cast<int>(MovementMode::kPursuit))
             : 0;
}

// decomp: func_800DB5B0 (src/BATTLE/BATTLE.PRG/72B70.s:182).
void Battle_Ai_WanderAroundSavedCenter(void* view) {
  Require(view != nullptr, "AI wandering requires state");
  auto& state = *static_cast<Battle_Ai_ActorAiState*>(view);
  Battle_Ai_WanderAroundTile(
      reinterpret_cast<Battle_Ai_CandidateMovementView*>(&state),
      state.wander_center.word, state.wander_center_radius);
}

// decomp: func_800DB5B8 (src/BATTLE/BATTLE.PRG/72B70.s:192).
void Battle_Ai_WanderAroundTile(Battle_Ai_CandidateMovementView* view,
                                unsigned int tile, int radius) {
  Require(view != nullptr && radius >= 0, "AI wander radius is invalid");
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  Battle_Ai_SetMovementBehavior(&state,
                                static_cast<int>(MovementMode::kWander));
  Battle_Ai_UpdateWanderMovement(
      reinterpret_cast<Battle_Ai_WanderView*>(&state), tile,
      static_cast<unsigned>(radius));
}

// decomp: func_800DB5DC (src/BATTLE/BATTLE.PRG/72B70.s:209);
// completion tail L800E5030 (7C7E0.s).
void Battle_Ai_MoveToTile(Battle_Ai_CandidateMovementView* view,
                          unsigned int tile, int radius) {
  Require(view != nullptr && radius >= 0, "AI tile movement radius is invalid");
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  state.destination.word = tile;
  Battle_Ai_SetMovementBehavior(&state,
                                static_cast<int>(MovementMode::kDestination));
  constexpr unsigned kTileMask = 0xFF;
  constexpr int kTileSize = 128;
  constexpr int kTileCenter = 64;
  const int x = static_cast<int>(tile & kTileMask) * kTileSize + kTileCenter;
  const int z =
      static_cast<int>((tile >> kTileZShift) & kTileMask) * kTileSize +
      kTileCenter;
  const auto distance = static_cast<unsigned>(Core_Math_SquaredVectorLength(
      x - state.position.vx,
      Field_GroundHeightAtPackedTile(tile) - state.position.vy,
      z - state.position.vz));
  const auto limit =
      static_cast<unsigned>(radius) * static_cast<unsigned>(radius);
  if (distance < limit) {
    Battle_Ai_IssueActorCommand(&state, 0);
  } else {
    Battle_Ai_DispatchAiLocomotionMode(&state, 0, 0);
  }
}

// decomp: func_800DB5FC (src/BATTLE/BATTLE.PRG/72B70.s:225).
void Battle_Ai_PatrolWaypoints(void* view) {
  Require(view != nullptr, "AI waypoint patrol requires state");
  auto& state = *static_cast<Battle_Ai_ActorAiState*>(view);
  Battle_Ai_SetMovementBehavior(&state,
                                static_cast<int>(MovementMode::kPatrol));
  Battle_Ai_UpdateWaypointPatrol(
      reinterpret_cast<Battle_Ai_PatrolView*>(&state));
}

namespace {
template <typename Visit>
void VisitActors(Visit visit) {
  unsigned visited = 0;
  for (auto* actor = Battle_actors[0]; actor != nullptr; actor = actor->next) {
    Require(actor->id >= 0 && actor->id < kAiFrameActorSlots,
            "AI behavior actor ID is out of bounds");
    const unsigned bit = 1u << static_cast<unsigned>(actor->id);
    Require((visited & bit) == 0, "AI behavior actor list contains a cycle");
    visited |= bit;
    visit(*actor);
  }
}
}  // namespace

// decomp: func_800DB61C (src/BATTLE/BATTLE.PRG/72B70.s:239).
int Battle_Ai_FleeVisibleEnemies(Battle_Ai_CandidateMovementView* view,
                                 int kind) {
  Require(view != nullptr, "AI fleeing requires state");
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  unsigned x = 0, z = 0;
  int count = 0;
  VisitActors([&](const Battle_Actor& actor) {
    const auto& relation = state.relations[actor.id];
    if (relation.classification != kHostileClassification ||
        !relation.visible) {
      return;
    }
    const auto& position = TargetModel(actor.id).instance.position;
    x += static_cast<unsigned>(position.vx);
    z += static_cast<unsigned>(position.vz);
    if (++count > 1) {
      x >>= 1;
      z >>= 1;
    }
  });
  constexpr int kFleeMemory = 90;
  constexpr int kTileShift = 7;
  constexpr int kTileSize = 128;
  constexpr int kTileCenter = 64;
  if (count != 0) {
    state.flee_timer = kFleeMemory;
    state.flee_tile.word =
        (x >> kTileShift) | ((z >> kTileShift) << kTileZShift);
  } else {
    if (state.flee_timer != 0) --state.flee_timer;
    if (state.flee_timer == 0 ||
        state.currentMovementMode != static_cast<int>(MovementMode::kFlee)) {
      return 0;
    }
  }
  state.opaque4 = 1;
  state.destination = state.flee_tile;
  state.moveX =
      static_cast<int>(state.flee_tile.tile.tileX) * kTileSize + kTileCenter;
  state.moveZ =
      static_cast<int>(state.flee_tile.tile.tileZ) * kTileSize + kTileCenter;
  Battle_Ai_BeginAiMovementTowardPoint(&state, kind);
  return Battle_Ai_SetMovementBehavior(&state,
                                       static_cast<int>(MovementMode::kFlee));
}

// decomp: func_800DB74C (src/BATTLE/BATTLE.PRG/72B70.s:325).
int Battle_Ai_TrackActorPosition(Battle_Ai_ActorAiState* state, int target) {
  Require(state != nullptr && target >= 0 && target < kAiFrameActorSlots,
          "AI position tracking has an invalid target");
  const auto& model = TargetModel(static_cast<unsigned>(target)).instance;
  state->moveX = model.position.vx;
  state->moveZ = model.position.vz;
  state->destination = TargetTile(model, false);
  return Battle_Ai_UpdateTargetFacing(
             reinterpret_cast<Battle_Ai_MovementDecisionView*>(state),
             target) != 0
             ? Battle_Ai_SetMovementBehavior(
                   state, static_cast<int>(MovementMode::kFacing))
             : 0;
}

// decomp: func_800DBB2C (src/BATTLE/BATTLE.PRG/72B70.s:620).
int Battle_Ai_FindActorByRelationship(void* view, void* tile, int relation) {
  Require(view != nullptr && relation >= 0 && relation < kRelationshipClasses,
          "AI relationship query is invalid");
  auto& state = *static_cast<Battle_Ai_ActorAiState*>(view);
  int result = -1;
  VisitActors([&](const Battle_Actor& actor) {
    if (result >= 0 || actor.id == state.actorId ||
        state.relations[actor.id].classification !=
            static_cast<unsigned>(relation)) {
      return;
    }
    result = actor.id;
    if (tile != nullptr) {
      *static_cast<unsigned*>(tile) =
          TargetTile(TargetModel(actor.id).instance, false).word;
    }
  });
  return result;
}

// decomp: func_800E437C (src/BATTLE/BATTLE.PRG/7B5DC.s:451).
int Battle_Ai_TargetFacingCosine(SVECTOR* from, SVECTOR* to, int angle,
                                 u_int distance) {
  Require(from != nullptr && to != nullptr,
          "AI facing cosine requires native positions");
  constexpr int kQ12 = 4096;
  constexpr int kQuarterTurn = 1024;
  constexpr int kFacingOffset = 3072;
  constexpr int kQ12Shift = 12;
  constexpr int kFacingMask = 4095;
  if (distance == 0) return kQ12;
  const int facing = (kFacingOffset - angle) & kFacingMask;
  VECTOR direction{to->vx - from->vx, 0, to->vz - from->vz, 0};
  VECTOR unit{};
  Core_Gte_VectorNormal(&direction, &unit);
  const int dot = unit.vx * Core_Gte_Sin(facing + kQuarterTurn) +
                  unit.vz * Core_Gte_Sin(facing);
  return std::clamp(dot >> kQ12Shift, -kQ12, kQ12);
}

// decomp: func_800D9DD8 (src/BATTLE/BATTLE.PRG/70D4C.s:627).
void Battle_Ai_SelectActionMovementMode(
    Battle_Ai_CandidateReadinessView* view) {
  Require(view != nullptr, "AI action movement selection requires state");
  auto& state = *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
  const auto& settings = *reinterpret_cast<Battle_Ai_SettingsView*>(view);
  if (settings.decisionCallback == Battle_Ai_UpdatePatrolCombatBehavior ||
      settings.opaque2D == 0) {
    state.planMode = 1;
    return;
  }
  Require(state.candidateOrder[0] != nullptr,
          "AI action movement selection requires its best candidate");
  const auto flags = std::bit_cast<unsigned>(state.candidateOrder[0]->opaque10);
  if ((flags & settings.opaque2D) == 0) {
    state.planMode = 1;
  } else {
    state.planMode = 2;
    Battle_Ai_FindNearestActiveWaypoint(
        reinterpret_cast<Battle_Ai_WaypointSearchView*>(&state),
        reinterpret_cast<Battle_Ai_WaypointTile*>(&state.plannedTile));
  }
}

// decomp: func_800DC810 (src/BATTLE/BATTLE.PRG/73C84.s:290).
void Battle_Ai_ApplyActionRelationshipChanges(Battle_Ai_ReactionRecord* app) {
  // Actor creation passes no action; the second relationship pass still runs.
  VisitActors([&](const Battle_Actor& actor) {
    auto* state = Battle_Ai_actorAiStates[actor.id];
    if (state != nullptr) {
      Battle_Ai_ApplyActorActionReaction(
          reinterpret_cast<Battle_Ai_ReactionView*>(state),
          reinterpret_cast<Battle_Ai_StatApplicationView*>(app));
    }
  });
  VisitActors([&](const Battle_Actor& actor) {
    auto* state = Battle_Ai_actorAiStates[actor.id];
    if (state != nullptr) {
      Battle_Ai_PropagateActorRelationships(
          reinterpret_cast<Battle_Ai_ReactionView*>(state));
    }
  });
}
