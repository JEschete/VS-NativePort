// Hand-maintained (tools/port/owned.csv).
// Live binding for the room waypoints and patrols (Waypoints.cpp).
#include "Battle/Ai/WaypointsLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/TerrainHeight.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Battle::Ai {
namespace {
// The room's auxiliary section holds four patrol rows before its waypoints.
constexpr std::size_t kLivePatrolRows = 4;

static_assert(sizeof(VS::Battle::Ai::patrolRoutes[0]) ==
              kPatrolWaypointsPerRow * sizeof(std::uint16_t));
// The patrol and waypoint search views alias the actor AI state.
using AiState = VS::Battle::Ai::ActorAiState;
static_assert(offsetof(VS::Battle::Ai::PatrolView, tile) ==
              offsetof(AiState, tile));
static_assert(offsetof(VS::Battle::Ai::PatrolView, index) ==
              offsetof(AiState, patrol_index));
static_assert(offsetof(VS::Battle::Ai::PatrolView, cooldown) ==
              offsetof(AiState, patrol_cooldown));
static_assert(offsetof(VS::Battle::Ai::PatrolView, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(VS::Battle::Ai::WaypointSearchView, x) ==
              offsetof(AiState, position));

Field::TerrainGrid LiveSpecialHeights() {
  if (VS::Battle::Ai::specialTerrainHeights == nullptr) return {};
  return Field::TerrainGrid{
      std::span<const Field::TerrainRow, Field::kTerrainGridSide>(
          VS::Battle::Ai::specialTerrainHeights, Field::kTerrainGridSide)};
}

class LivePatrolRandom final : public PatrolRandom {
 public:
  int NextMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
};
}  // namespace

std::span<const std::uint16_t> LivePatrolRoutes() {
  if (VS::Battle::Ai::patrolRoutes == nullptr) return {};
  return {&VS::Battle::Ai::patrolRoutes[0][0],
          kLivePatrolRows * kPatrolWaypointsPerRow};
}

void FindLiveNearestActiveWaypoint(
    VS::Battle::Ai::WaypointSearchView* state,
    VS::Battle::Ai::WaypointTileRecord* destination) {
  Require(state != nullptr && destination != nullptr,
          "AI waypoint search requires an AI state and a destination");
  Require(VS::Battle::Ai::roomWaypoints != nullptr,
          "AI waypoint search requires the room's waypoints");
  const std::span<const std::uint16_t, kRoomWaypointCount> waypoints(
      VS::Battle::Ai::roomWaypoints, kRoomWaypointCount);
  const auto search =
      Required(FindNearestActiveWaypoint(
                   waypoints, {.x = state->x, .y = state->y, .z = state->z},
                   LiveSpecialHeights()),
               "AI waypoint search refused the room's special terrain");
  if (!search.nearest) return;
  destination->x = search.nearest->tile.x;
  destination->z = search.nearest->tile.z;
}

void UpdateLiveWaypointPatrol(VS::Battle::Ai::PatrolView* state) {
  Require(state != nullptr, "AI patrol requires an AI state");
  PatrolCursor cursor{.index = state->index,
                      .row = state->row,
                      .rate = state->rate,
                      .direction = state->direction,
                      .cooldown = state->cooldown};
  LivePatrolRandom random;
  const auto target =
      Required(UpdateWaypointPatrol(
                   cursor, LivePatrolRoutes(),
                   {.x = static_cast<std::uint8_t>(state->tile.tile.tile_x),
                    .z = static_cast<std::uint8_t>(state->tile.tile.tile_z)},
                   random),
               "AI patrol refused its route row");
  state->index = cursor.index;
  state->row = cursor.row;
  state->rate = cursor.rate;
  state->direction = cursor.direction;
  state->cooldown = cursor.cooldown;
  state->destination.tile.tile_x = target.x;
  state->destination.tile.tile_z = target.z;
  DispatchLiveAiLocomotionMode(state, 0, 0);
}
}  // namespace VS::Battle::Ai
