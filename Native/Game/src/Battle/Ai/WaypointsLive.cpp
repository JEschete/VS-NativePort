// Hand-maintained (tools/port/owned.csv).
// Live binding for the room waypoints and patrols (Waypoints.cpp).
#include "Battle/Ai/WaypointsLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Field/TerrainHeight.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
// The room's auxiliary section holds four patrol rows before its waypoints.
constexpr std::size_t kLivePatrolRows = 4;

static_assert(sizeof(Battle_Ai_patrolRoutes[0]) ==
              kPatrolWaypointsPerRow * sizeof(std::uint16_t));
// The patrol and waypoint search views alias the actor AI state.
using AiState = Battle_Ai_ActorAiState;
static_assert(offsetof(Battle_Ai_PatrolView, tile) == offsetof(AiState, tile));
static_assert(offsetof(Battle_Ai_PatrolView, index) ==
              offsetof(AiState, patrolIndex));
static_assert(offsetof(Battle_Ai_PatrolView, opaque104) ==
              offsetof(AiState, patrolCooldown));
static_assert(offsetof(Battle_Ai_PatrolView, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(Battle_Ai_WaypointSearchView, x) ==
              offsetof(AiState, position));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

template <typename T>
T Required(const std::optional<T>& value, const char* message) {
  Require(value.has_value(), message);
  return *value;
}

Field::TerrainGrid LiveSpecialHeights() {
  if (Battle_Ai_specialTerrainHeights == nullptr) return {};
  return Field::TerrainGrid{
      std::span<const Field::TerrainRow, Field::kTerrainGridSide>(
          Battle_Ai_specialTerrainHeights, Field::kTerrainGridSide)};
}

class LivePatrolRandom final : public PatrolRandom {
 public:
  int NextMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
};
}  // namespace

std::span<const std::uint16_t> LivePatrolRoutes() {
  if (Battle_Ai_patrolRoutes == nullptr) return {};
  return {&Battle_Ai_patrolRoutes[0][0],
          kLivePatrolRows * kPatrolWaypointsPerRow};
}

void FindLiveNearestActiveWaypoint(Battle_Ai_WaypointSearchView* state,
                                   Battle_Ai_WaypointTile* destination) {
  Require(state != nullptr && destination != nullptr,
          "AI waypoint search requires an AI state and a destination");
  Require(Battle_Ai_roomWaypoints != nullptr,
          "AI waypoint search requires the room's waypoints");
  const std::span<const std::uint16_t, kRoomWaypointCount> waypoints(
      Battle_Ai_roomWaypoints, kRoomWaypointCount);
  const auto search =
      Required(FindNearestActiveWaypoint(
                   waypoints, {.x = state->x, .y = state->y, .z = state->z},
                   LiveSpecialHeights()),
               "AI waypoint search refused the room's special terrain");
  if (!search.nearest) return;
  destination->x = search.nearest->tile.x;
  destination->z = search.nearest->tile.z;
}

void UpdateLiveWaypointPatrol(Battle_Ai_PatrolView* state) {
  Require(state != nullptr, "AI patrol requires an AI state");
  PatrolCursor cursor{.index = state->index,
                      .row = state->row,
                      .rate = state->rate,
                      .direction = state->direction,
                      .cooldown = state->opaque104};
  LivePatrolRandom random;
  const auto target =
      Required(UpdateWaypointPatrol(
                   cursor, LivePatrolRoutes(),
                   {.x = static_cast<std::uint8_t>(state->tile.tile.tileX),
                    .z = static_cast<std::uint8_t>(state->tile.tile.tileZ)},
                   random),
               "AI patrol refused its route row");
  state->index = cursor.index;
  state->row = cursor.row;
  state->rate = cursor.rate;
  state->direction = cursor.direction;
  state->opaque104 = cursor.cooldown;
  state->destination.tile.tileX = target.x;
  state->destination.tile.tileZ = target.z;
  DispatchLiveAiLocomotionMode(state, 0, 0);
}
}  // namespace VS::Battle::Ai
