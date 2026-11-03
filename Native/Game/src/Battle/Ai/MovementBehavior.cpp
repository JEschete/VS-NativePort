// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:23-85,386-425,1513-1553,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/MovementBehavior.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>

#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
namespace {
constexpr int kPursuitResult = 3;
constexpr int kWanderResult = 6;
constexpr int kPatrolResult = 1;
constexpr std::size_t kModeCounterStride = 2;
constexpr std::uint32_t kResetNavigationBits = 0xFE1;
constexpr std::uint32_t kEnabledNavigationBits = 0x3C0;
constexpr std::uint16_t kEnabledWaypoint = 0x8000;
constexpr unsigned kWaypointCoordinateMask = 31;
constexpr unsigned kWaypointZShift = 5;
constexpr int kTargetHalfTileShift = 6;
constexpr std::uint16_t kBusyMovementMask = 0x38;
constexpr unsigned kPlatformLayerBase = 2;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kTileXZMask = 0x00FF00FF;
constexpr unsigned kRelationVisibleShift = 5;
constexpr std::uint32_t kVisibleHostile = 7;
constexpr std::uint32_t kRelationClassMask = 0xC0;
constexpr unsigned kRelationClassShift = 6;
constexpr std::uint16_t kFleeMemoryFrames = 90;
constexpr unsigned kWorldTileShift = 7;
constexpr int kTileSize = 128;
constexpr int kTileCenter = 64;

std::nullopt_t Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return std::nullopt;
}

bool IsActorSlot(int actor_id) {
  return actor_id >= 0 && actor_id < kMovementActorSlots;
}

std::uint32_t PackTileXZ(unsigned x, unsigned z) {
  return PackedTileX(x) | (PackedTileX(z) << kTileZShift);
}

std::optional<std::span<const std::uint16_t>> PatrolRow(
    std::span<const std::uint16_t> routes, std::uint8_t row) {
  const std::size_t first = std::size_t{row} * kPatrolWaypointsPerRow;
  if (first + kPatrolWaypointsPerRow > routes.size()) {
    return Refuse("AI patrol row is outside the room's waypoint routes");
  }
  return routes.subspan(first, kPatrolWaypointsPerRow);
}

// Picks the enabled waypoint nearest the actor's tile by Manhattan distance.
void SelectPatrolWaypoint(MovementState& state,
                          std::span<const std::uint16_t> row) {
  const int tile_x = static_cast<int>(PackedTileX(state.tile));
  const int tile_z = static_cast<int>(PackedTileZ(state.tile));
  unsigned nearest = std::numeric_limits<unsigned>::max();
  for (std::size_t index = 0; index < row.size(); ++index) {
    const unsigned entry = row[index];
    if ((entry & kEnabledWaypoint) == 0) continue;
    const int x = static_cast<int>(entry & kWaypointCoordinateMask);
    const int z =
        static_cast<int>((entry >> kWaypointZShift) & kWaypointCoordinateMask);
    const auto distance =
        static_cast<unsigned>(std::abs(x - tile_x) + std::abs(z - tile_z));
    if (distance < nearest) {
      nearest = distance;
      state.patrol.index = static_cast<std::uint8_t>(index);
    }
  }
  state.patrol.cooldown = 0;
}

// Enters `mode`, whose patrol row (for kPatrol) was already validated.
int EnterMode(MovementState& state, MovementMode mode,
              std::span<const std::uint16_t> patrol_row = {}) {
  const auto value = static_cast<std::uint8_t>(mode);
  state.requested_mode = value;
  if (state.current_mode == value) return value;
  int result = -1;
  switch (mode) {
    case MovementMode::kPursuit: {
      const auto kept = state.pursuit;
      state.pursuit = PursuitProgress{};
      state.pursuit.previous_option = kept.previous_option;
      state.pursuit.previous_target_id = kept.previous_target_id;
      result = kPursuitResult;
      break;
    }
    case MovementMode::kWander:
      state.wander_phase = 0;
      state.wander_cooldown = 0;
      result = kWanderResult;
      break;
    case MovementMode::kPatrol:
      SelectPatrolWaypoint(state, patrol_row);
      result = kPatrolResult;
      break;
    case MovementMode::kFacing:
      state.previous_facing_tile = std::numeric_limits<std::uint32_t>::max();
      state.facing_pending = 0;
      break;
    default:
      break;
  }
  state.current_mode = value;
  state.mode_counters.at(std::size_t{value} * kModeCounterStride) = 0;
  state.navigation_flags =
      RefreshAiMovementCooldown(state.navigation_flags, state.movement_enabled);
  return result;
}

std::optional<RouteGate> PlatformGate(const MovementHost& host,
                                      unsigned layer) {
  if (layer < kPlatformLayerBase ||
      layer - kPlatformLayerBase >=
          static_cast<unsigned>(kPlatformGroupCount)) {
    return Refuse("AI target stands on an unknown moving-platform layer");
  }
  const auto gate =
      host.PlatformRouteGate(static_cast<int>(layer - kPlatformLayerBase));
  if (!gate) {
    return Refuse("AI platform targeting requires waypoint and route gates");
  }
  return gate;
}

std::optional<TargetPlacement> RequirePlacement(const MovementHost& host,
                                                int actor_id) {
  auto placement = host.Placement(actor_id);
  if (!placement) return Refuse("AI movement target has no model");
  return placement;
}
}  // namespace

// decomp: func_800E4BB8 (src/BATTLE/BATTLE.PRG/7BDD4.s:497).
std::uint32_t RefreshAiMovementCooldown(std::uint32_t navigation_flags,
                                        bool movement_enabled) {
  navigation_flags &= ~kResetNavigationBits;
  if (movement_enabled) navigation_flags |= kEnabledNavigationBits;
  return navigation_flags;
}

// decomp: func_800DB4AC (src/BATTLE/BATTLE.PRG/72B70.s:102).
std::optional<int> SetMovementBehavior(
    MovementState& state, std::span<const std::uint16_t> patrol_routes,
    int mode) {
  if (mode < 0 || mode >= kMovementModeCount) {
    return Refuse("AI movement mode is outside the behaviour table");
  }
  const auto movement_mode = static_cast<MovementMode>(mode);
  std::span<const std::uint16_t> row;
  if (movement_mode == MovementMode::kPatrol && state.current_mode != mode) {
    const auto patrol_row = PatrolRow(patrol_routes, state.patrol.row);
    if (!patrol_row) return std::nullopt;
    row = *patrol_row;
  }
  return EnterMode(state, movement_mode, row);
}

// decomp: func_800E4904 (src/BATTLE/BATTLE.PRG/7BDD4.s:279).
bool RefreshAiTargetTile(MovementState& state, const MovementHost& host) {
  const int target = state.pursuit.target_id;
  if (!IsActorSlot(target)) {
    Refuse("AI pursuit target ID is outside the actor slots");
    return false;
  }
  if (!host.HasFrameActor(target)) return true;
  const auto placement = RequirePlacement(host, target);
  if (!placement) return false;
  const bool planned = placement->planned_move;
  const unsigned layer = PackedTileLayer(planned ? placement->planned_tile
                                                 : placement->current_tile);
  unsigned x = 0;
  unsigned z = 0;
  if ((!planned && layer >= kPlatformLayerBase) || (planned && layer != 0)) {
    const auto gate = PlatformGate(host, layer);
    if (!gate) return false;
    x = gate->x * 2u;
    z = gate->z * 2u;
  } else {
    x = static_cast<unsigned>((planned ? placement->planned_x : placement->x) >>
                              kTargetHalfTileShift);
    z = static_cast<unsigned>((planned ? placement->planned_z : placement->z) >>
                              kTargetHalfTileShift);
  }
  state.target_half_tile =
      (state.target_half_tile & ~kTileXZMask) | PackTileXZ(x, z);
  return true;
}

// decomp: func_800E48F8 (src/BATTLE/BATTLE.PRG/7BDD4.s:270).
bool RefreshAiTargetTileWhenTracking(MovementState& state,
                                     const MovementHost& host) {
  if (state.requested_mode != static_cast<int>(MovementMode::kPursuit)) {
    return true;
  }
  return RefreshAiTargetTile(state, host);
}

// decomp: func_800DB370 (src/BATTLE/BATTLE.PRG/72B70.s:11).
std::optional<int> ApproachActor(MovementState& state, MovementHost& host,
                                 const ApproachRequest& request) {
  const int target = request.target;
  if (!IsActorSlot(target)) {
    return Refuse("AI approach target ID is outside the actor slots");
  }
  if (!host.HasFrameActor(target)) {
    return target != 0 ? EnterMode(state, MovementMode::kPursuit) : 0;
  }
  const auto placement = RequirePlacement(host, target);
  if (!placement) return std::nullopt;
  const bool busy = (state.movement_flags & kBusyMovementMask) != 0;
  std::uint32_t destination = state.destination;
  unsigned layer = 0;
  if (!busy && placement->planned_move) {
    destination = placement->planned_tile;
    layer = PackedTileLayer(destination);
  } else if ((state.follows_platform_targets || busy) &&
             placement->support_layer != 0) {
    layer = placement->support_layer;
  } else {
    destination = placement->current_tile;
  }
  std::optional<RouteGate> gate;
  if (layer != 0) {
    gate = PlatformGate(host, layer);
    if (!gate) return std::nullopt;
  }
  if (state.pursuit.target_id != target) state.pursuit = PursuitProgress{};
  state.pursuit.target_id = static_cast<std::uint8_t>(target);
  state.approach_range = request.distance;
  if (gate) {
    destination = (destination & ~kTileXZMask) | PackTileXZ(gate->x, gate->z);
    state.cached_destination = destination;
    // The original stores the two doubled coordinates OR-combined in one word.
    state.target_half_tile =
        (std::uint32_t{gate->x} << 1) | (std::uint32_t{gate->z} << 1);
  }
  state.destination = destination;
  return host.UpdateTargetApproach(state, request.option) != 0
             ? EnterMode(state, MovementMode::kPursuit)
             : 0;
}

// decomp: func_800DB5B8 (src/BATTLE/BATTLE.PRG/72B70.s:192).
void WanderAroundTile(MovementState& state, MovementHost& host,
                      std::uint32_t tile, int radius) {
  EnterMode(state, MovementMode::kWander);
  host.UpdateWander(state, tile, static_cast<std::uint32_t>(radius));
}

// decomp: func_800DB5B0 (src/BATTLE/BATTLE.PRG/72B70.s:182).
void WanderAroundSavedCenter(MovementState& state, MovementHost& host) {
  WanderAroundTile(state, host, state.wander_center,
                   state.wander_center_radius);
}

// decomp: func_800DB5DC (src/BATTLE/BATTLE.PRG/72B70.s:209);
// completion tail L800E5030 (src/BATTLE/BATTLE.PRG/7C7E0.s:34).
void MoveToTile(MovementState& state, MovementHost& host, std::uint32_t tile,
                int radius) {
  state.destination = tile;
  EnterMode(state, MovementMode::kDestination);
  const int x = static_cast<int>(PackedTileX(tile)) * kTileSize + kTileCenter;
  const int z = static_cast<int>(PackedTileZ(tile)) * kTileSize + kTileCenter;
  const std::uint32_t distance = Core::Math::SquaredVectorLength(
      x - state.position.x, host.TileHeight(tile) - state.position.y,
      z - state.position.z);
  const auto limit =
      static_cast<std::uint32_t>(radius) * static_cast<std::uint32_t>(radius);
  if (distance < limit) {
    host.IssueCommand(state, 0);
  } else {
    host.DispatchLocomotion(state);
  }
}

// decomp: func_800DB5FC (src/BATTLE/BATTLE.PRG/72B70.s:225). The original
// skips the patrol when its third argument register is zero; every native
// caller patrols.
bool PatrolWaypoints(MovementState& state, MovementHost& host) {
  if (!SetMovementBehavior(state, host.PatrolRoutes(),
                           static_cast<int>(MovementMode::kPatrol))) {
    return false;
  }
  host.UpdateWaypointPatrol(state);
  return true;
}

// decomp: func_800DB61C (src/BATTLE/BATTLE.PRG/72B70.s:239).
std::optional<int> FleeVisibleEnemies(MovementState& state, MovementHost& host,
                                      int kind) {
  const auto roster = host.BattleActors();
  if (!roster) return Refuse("AI flight requires a valid battle actor list");
  std::uint32_t x = 0;
  std::uint32_t z = 0;
  int count = 0;
  for (const std::uint8_t id : roster->Ids()) {
    const std::uint32_t relation = state.relations.at(id);
    if (((relation >> kRelationVisibleShift) & kVisibleHostile) !=
        kVisibleHostile) {
      continue;
    }
    const auto placement = RequirePlacement(host, id);
    if (!placement) return std::nullopt;
    x += static_cast<std::uint32_t>(placement->x);
    z += static_cast<std::uint32_t>(placement->z);
    if (++count > 1) {
      x >>= 1;
      z >>= 1;
    }
  }
  if (count != 0) {
    state.flee_timer = kFleeMemoryFrames;
    state.flee_tile =
        (x >> kWorldTileShift) | ((z >> kWorldTileShift) << kTileZShift);
  } else {
    if (state.flee_timer != 0) --state.flee_timer;
    if (state.flee_timer == 0 ||
        state.current_mode != static_cast<int>(MovementMode::kFlee)) {
      return 0;
    }
  }
  state.arrive_when_near = 1;
  state.destination = state.flee_tile;
  state.move_x =
      static_cast<int>(PackedTileX(state.flee_tile)) * kTileSize + kTileCenter;
  state.move_z =
      static_cast<int>(PackedTileZ(state.flee_tile)) * kTileSize + kTileCenter;
  host.BeginMovementTowardPoint(state, kind);
  return EnterMode(state, MovementMode::kFlee);
}

// decomp: func_800DB74C (src/BATTLE/BATTLE.PRG/72B70.s:325).
std::optional<int> TrackActorPosition(MovementState& state, MovementHost& host,
                                      int target) {
  if (!IsActorSlot(target)) {
    return Refuse("AI facing target ID is outside the actor slots");
  }
  const auto placement = RequirePlacement(host, target);
  if (!placement) return std::nullopt;
  state.move_x = placement->x;
  state.move_z = placement->z;
  state.destination = placement->current_tile;
  return host.UpdateTargetFacing(state, target) != 0
             ? EnterMode(state, MovementMode::kFacing)
             : 0;
}

// decomp: func_800DBB2C (src/BATTLE/BATTLE.PRG/72B70.s:620).
std::optional<RelatedActor> FindActorByRelationship(const MovementState& state,
                                                    const MovementHost& host,
                                                    int relation,
                                                    bool want_tile) {
  const auto roster = host.BattleActors();
  if (!roster) {
    return Refuse("AI relationship query requires a valid battle actor list");
  }
  const std::uint32_t wanted = static_cast<std::uint32_t>(relation)
                               << kRelationClassShift;
  for (const std::uint8_t id : roster->Ids()) {
    if (id == state.actor_id ||
        (state.relations.at(id) & kRelationClassMask) != wanted) {
      continue;
    }
    RelatedActor related{.actor_id = id};
    if (want_tile) {
      const auto placement = RequirePlacement(host, id);
      if (!placement) return std::nullopt;
      related.tile = placement->current_tile;
    }
    return related;
  }
  return RelatedActor{};
}

// decomp: func_800E437C (src/BATTLE/BATTLE.PRG/7B5DC.s:451). The direction
// is scaled with func_80041AF4's reciprocal-root table.
int TargetFacingCosine(Core::Math::Position3 from, Core::Math::Position3 to,
                       int angle, std::uint32_t distance) {
  constexpr int kQ12 = 4096;
  constexpr std::uint32_t kFacingOffset = 0xC00;
  constexpr std::uint32_t kAngleMask = 0xFFF;
  constexpr int kQuarterTurn = 0x400;
  constexpr int kQ12Shift = 12;
  if (distance == 0) return kQ12;
  const auto facing = static_cast<int>(
      (kFacingOffset - static_cast<std::uint32_t>(angle)) & kAngleMask);
  const Core::Math::Vec3 direction{to.x - from.x, 0, to.z - from.z};
  const Core::Math::Vec3 unit =
      Core::Math::NormalizeVector(direction).direction;
  const int dot = unit.x * Core::Math::Sine(facing + kQuarterTurn) +
                  unit.z * Core::Math::Sine(facing);
  return std::clamp(dot >> kQ12Shift, -kQ12, kQ12);
}

// decomp: func_800D9DD8 (src/BATTLE/BATTLE.PRG/70D4C.s:627).
bool SelectActionMovementMode(MovementState& state, MovementHost& host) {
  constexpr std::uint8_t kMoveInPlace = 1;
  constexpr std::uint8_t kWalkToWaypoint = 2;
  const std::uint8_t mask = host.ActionMovementMask();
  // A zero mask cannot match the best candidate, so it is not read.
  if (host.RunsPatrolCombatBehavior() || mask == 0) {
    state.plan_mode = kMoveInPlace;
    return true;
  }
  const auto flags = host.BestCandidateFlags();
  if (!flags) {
    Refuse("AI action movement selection requires its best candidate");
    return false;
  }
  if ((*flags & mask) == 0) {
    state.plan_mode = kMoveInPlace;
  } else {
    state.plan_mode = kWalkToWaypoint;
    host.FindNearestActiveWaypoint(state);
  }
  return true;
}

// decomp: func_800DC810 (src/BATTLE/BATTLE.PRG/73C84.s:290).
bool ApplyActionRelationshipChanges(RelationshipChangeHost& host) {
  const auto roster = host.BattleActors();
  if (!roster) {
    Refuse("AI relationship changes require a valid battle actor list");
    return false;
  }
  // The original reads each listed actor's state without a null check.
  for (const std::uint8_t id : roster->Ids()) {
    if (host.HasAiState(id)) host.ApplyActionReaction(id);
  }
  for (const std::uint8_t id : roster->Ids()) {
    if (host.HasAiState(id)) host.PropagateRelationships(id);
  }
  return true;
}
}  // namespace VS::Battle::Ai
