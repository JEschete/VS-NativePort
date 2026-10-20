// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:23-85,386-425,1513-1553,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/MovementBehavior.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Field/Gimmicks.h"
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
