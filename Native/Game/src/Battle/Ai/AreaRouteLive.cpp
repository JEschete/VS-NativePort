// Hand-maintained (tools/port/owned.csv).
// Live binding for the area routing (AreaRoute.cpp).
#include "Battle/Ai/AreaRouteLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>

#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using RouteView = Battle_Ai_AreaRouteView;
using AiState = Battle_Ai_ActorAiState;

constexpr std::size_t kTileWordBytes = sizeof(std::uint32_t);
// A gate tile's x, platform and z bytes.
constexpr std::size_t kRouteGateTileBytes = 3;
constexpr unsigned kHalfwordShift = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;

// The routing view aliases the actor AI state it is handed.
static_assert(offsetof(RouteView, opaque5) == offsetof(AiState, opaque5));
static_assert(offsetof(RouteView, tile) == offsetof(AiState, tile));
static_assert(offsetof(RouteView, x) == offsetof(AiState, position));
static_assert(offsetof(RouteView, nativePointer60) ==
              offsetof(AiState, nativePointer60));
static_assert(offsetof(RouteView, actorId) == offsetof(AiState, actorId));
static_assert(offsetof(RouteView, cooldown) == offsetof(AiState, opaque13A));
static_assert(offsetof(RouteView, period) == offsetof(AiState, opaque13C));
static_assert(offsetof(RouteView, area) == offsetof(AiState, reserved154Area));
static_assert(offsetof(RouteView, movementTarget) ==
              offsetof(AiState, movementTarget));
static_assert(offsetof(RouteView, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(RouteView, nextTile) == offsetof(AiState, nextTile));
static_assert(offsetof(RouteView, moveX) == offsetof(AiState, moveX));
static_assert(offsetof(RouteView, moveZ) == offsetof(AiState, moveZ));
static_assert(offsetof(RouteView, heading) == offsetof(AiState, heading));
static_assert(offsetof(RouteView, gate) == offsetof(AiState, route_gate));
static_assert(offsetof(RouteView, exit) == offsetof(AiState, route_exit_bytes));
static_assert(sizeof(AiState{}.route_exit_bytes) == kTileWordBytes);
static_assert(sizeof(Battle_Ai_TilePosition) == kTileWordBytes);
static_assert(kAiFrameActorSlots == kAreaRouteNoTarget);
static_assert(kAiFrameGridSide == kNavigationGridSide);
static_assert(kAiDirectionBytes == kNavigationDirections);
static_assert(sizeof(Battle_Ai_RouteGateTile) == kRouteGateTileBytes);
// So does the area movement view.
using MoveView = Battle_Ai_TerrainMoveView;
static_assert(offsetof(MoveView, isStuck) == offsetof(AiState, isStuck));
static_assert(offsetof(MoveView, opaque4) == offsetof(AiState, opaque4));
static_assert(offsetof(MoveView, opaque5) == offsetof(AiState, opaque5));
static_assert(offsetof(MoveView, opaque1A) == offsetof(AiState, opaque1A));
static_assert(offsetof(MoveView, settingsBit14) ==
              offsetof(AiState, settingsBit14));
static_assert(offsetof(MoveView, tile) == offsetof(AiState, tile));
static_assert(offsetof(MoveView, position) == offsetof(AiState, position));
static_assert(offsetof(MoveView, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(MoveView, moveX) == offsetof(AiState, moveX));
static_assert(offsetof(MoveView, moveZ) == offsetof(AiState, moveZ));
static_assert(offsetof(MoveView, angle) == offsetof(AiState, heading));

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

AiState& State(void* view) {
  Require(view != nullptr, "AI area routing requires an AI state");
  return *static_cast<AiState*>(view);
}

Battle_Ai_FrameContext& Frame() { return *Battle_Ai_FrameState(); }

std::optional<std::array<RouteGateEnd, kRouteGateEndCount>> LiveGateEnds() {
  if (Battle_Ai_routeGates == nullptr) return std::nullopt;
  std::array<RouteGateEnd, kRouteGateEndCount> ends{};
  for (std::size_t index = 0; index < ends.size(); ++index) {
    ends[index] = {
        .x = static_cast<std::uint8_t>(Battle_Ai_routeGates[index].tileX),
        .z = static_cast<std::uint8_t>(Battle_Ai_routeGates[index].tileZ)};
  }
  return ends;
}

AreaRouteActor Load(const AiState& state) {
  std::uint32_t exit = 0;
  std::memcpy(&exit, state.route_exit_bytes, sizeof(exit));
  return {.target_distance_filter = state.opaque5,
          .tile = state.tile.word,
          .x = state.position.vx,
          .z = state.position.vz,
          .actor_id = state.actorId,
          .turn_timer = state.opaque13A,
          .turn_period = state.opaque13C,
          .area = state.reserved154Area,
          .movement_target = state.movementTarget,
          .destination = state.destination.word,
          .next_tile = state.nextTile.word,
          .move_x = state.moveX,
          .move_z = state.moveZ,
          .heading = state.heading,
          .angle = static_cast<std::int16_t>(state.angle),
          .route_gate = state.route_gate,
          .route_exit = exit};
}

void Store(const AreaRouteActor& actor, AiState& state) {
  state.opaque5 = actor.target_distance_filter;
  state.tile.word = actor.tile;
  state.position.vx = actor.x;
  state.position.vz = actor.z;
  state.actorId = actor.actor_id;
  state.opaque13A = actor.turn_timer;
  state.opaque13C = actor.turn_period;
  state.reserved154Area = actor.area;
  state.movementTarget = actor.movement_target;
  state.destination.word = actor.destination;
  state.nextTile.word = actor.next_tile;
  state.moveX = actor.move_x;
  state.moveZ = actor.move_z;
  state.heading = actor.heading;
  state.angle = actor.angle;
  state.route_gate = actor.route_gate;
  std::memcpy(state.route_exit_bytes, &actor.route_exit,
              sizeof(actor.route_exit));
}

// The room's AI frame, route table and platforms, and the actor's state for
// the occupancy query.
class LiveAreaRouteWorld final : public AreaRouteWorld {
 public:
  explicit LiveAreaRouteWorld(AiState& state) : state_(state) {}

  [[nodiscard]] std::optional<AreaRouteEndpoints> Route(
      int index) const override {
    if (Frame().roomData == nullptr) return std::nullopt;
    const auto word = static_cast<const u_int*>(
        Frame().roomData)[static_cast<std::size_t>(index)];
    return AreaRouteEndpoints{
        static_cast<std::uint16_t>(word & kHalfwordMask),
        static_cast<std::uint16_t>(word >> kHalfwordShift)};
  }
  [[nodiscard]] bool HasTileMaps() const override {
    return Frame().tileFlags != nullptr && Frame().heightMap != nullptr;
  }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return Frame().tileFlags[z][x];
  }
  [[nodiscard]] std::uint16_t TileHeight(int x, int z) const override {
    return Frame().heightMap[z][x];
  }
  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const override {
    u_char* cell = nullptr;
    if (Frame().terrainRegions == nullptr ||
        Battle_Ai_TryTerrainRegionCell(x, z, &cell) == 0) {
      return std::nullopt;
    }
    return *cell;
  }
  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    NavigationDirectionCodes codes{};
    std::memcpy(codes.data(), Frame().directions, codes.size());
    return codes;
  }
  [[nodiscard]] std::uint8_t RouteStepDirection(int kind) const override {
    return Battle_Ai_routeStepDirections[static_cast<std::size_t>(kind)];
  }
  [[nodiscard]] std::optional<std::array<RouteGateEnd, kRouteGateEndCount>>
  RouteGateEnds() const override {
    return LiveGateEnds();
  }
  [[nodiscard]] std::vector<Field::PlatformContact> PlatformPositions()
      const override {
    return Field::CollectLiveMovingPlatformPositions();
  }
  [[nodiscard]] std::optional<std::uint16_t> PlatformDwellTicks(
      int platform) const override {
    return Field::LiveMovingPlatformDwellTicks(platform);
  }
  [[nodiscard]] std::optional<std::uint8_t> ClosedGates() const override {
    if (Battle_Ai_roomAiState == nullptr) return std::nullopt;
    return Battle_Ai_roomAiState->reservedEPrefix[0];
  }
  void SetContactOccupancy(int bit) override { SetLiveAiContactOccupancy(bit); }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      const AreaRouteActor& actor, std::uint32_t tile) override {
    Store(actor, state_);
    return LiveDestinationOccupancy(
        reinterpret_cast<Battle_Ai_MoveTargetView*>(&state_), tile);
  }
  [[nodiscard]] bool HasActor(int slot) const override {
    return Frame().actors[slot] != nullptr;
  }
  [[nodiscard]] std::optional<AreaRoutePoint> ActorPosition(
      int slot) const override {
    const auto* position =
        static_cast<const SVECTOR*>(Frame().actors[slot]->nativePointer60);
    if (position == nullptr) return std::nullopt;
    return AreaRoutePoint{.x = position->vx, .z = position->vz};
  }

 private:
  AiState& state_;
};

std::uint32_t TileWord(const Battle_Ai_RouteGateTile* tile) {
  Require(tile != nullptr, "AI route links require a tile");
  // The original read the whole word behind the three tile bytes.
  std::uint32_t word = 0;
  std::memcpy(&word, tile, sizeof(word));
  return word;
}
}  // namespace

std::optional<RouteGateEnd> FindLiveNearestRouteGateEnd(int platform,
                                                        std::int32_t world_x,
                                                        std::int32_t world_z) {
  const auto ends = LiveGateEnds();
  if (!ends) return std::nullopt;
  return FindNearestRouteGateEntry(*ends, platform, world_x, world_z);
}

std::optional<int> FindLiveRouteGateLink(std::uint32_t tile,
                                         bool match_entry_end) {
  const auto ends = LiveGateEnds();
  if (!ends) return std::nullopt;
  const auto positions = Field::CollectLiveMovingPlatformPositions();
  return FindRouteGateAtTile(*ends, positions, tile, match_entry_end);
}

void ResolveLiveRouteGateExit(Battle_Ai_RouteGateTile* entry_tile,
                              Battle_Ai_RouteGateTile* exit_tile) {
  Require(exit_tile != nullptr, "AI route link exit requires a tile");
  const auto entry = TileWord(entry_tile);
  const auto ends =
      Required(LiveGateEnds(), "AI route link exit requires the room's links");
  const auto exit = ResolveRouteGateExit(ends, entry);
  if (!exit) {
    std::memcpy(exit_tile, &entry, sizeof(entry));
    return;
  }
  exit_tile->opaque0 = exit->x;
  exit_tile->opaque1 = exit->platform;
  exit_tile->opaque2 = exit->z;
}

void FindLiveNearestRouteGateEntry(int platform, int world_x, int world_z,
                                   Battle_Ai_RouteGateTile* entry_tile) {
  Require(entry_tile != nullptr, "AI route link entry requires a tile");
  const auto end =
      Required(FindLiveNearestRouteGateEnd(platform, world_x, world_z),
               "AI route link entry refused its platform or room");
  entry_tile->opaque0 = end.x;
  entry_tile->opaque2 = end.z;
}

int FindLiveRouteGateAtTile(std::intptr_t /*state*/, unsigned int tile,
                            int match_entry_end) {
  return Required(FindLiveRouteGateLink(tile, match_entry_end != 0),
                  "AI route link lookup refused its room or platforms");
}

int ResolveLiveNavigationAreaRoute(Battle_Ai_AreaRouteView* view, int index,
                                   int area) {
  auto& state = State(view);
  LiveAreaRouteWorld world(state);
  auto actor = Load(state);
  const auto intent = ResolveNavigationAreaRoute(world, actor, index, area);
  Store(actor, state);
  return static_cast<int>(
      Required(intent, "AI area route refused its room or actor"));
}

void UpdateLiveNavigationAreaMovement(Battle_Ai_TerrainMoveView* view) {
  auto& state = State(view);
  LiveAreaRouteWorld world(state);
  AreaMovementActor actor{.route = Load(state),
                          .is_stuck = state.isStuck,
                          .near_target_detour = state.opaque4,
                          .height_filtered_movement = state.opaque1A,
                          .area_lock = state.settingsBit14};
  const auto step = UpdateNavigationAreaMovement(world, actor);
  Store(actor.route, state);
  state.isStuck = actor.is_stuck;
  state.opaque4 = actor.near_target_detour;
  const auto movement =
      Required(step, "AI area movement refused its room or actor");
  switch (movement.action) {
    case AreaMovementAction::kHeightFilteredDirections:
      Battle_Ai_MoveWithHeightFilteredDirections(view);
      return;
    case AreaMovementAction::kGroundMovement:
      RefreshLiveAiMovementCooldown(view);
      BeginLiveAiGroundMovement(&state, movement.value);
      return;
    case AreaMovementAction::kCommand:
      RefreshLiveAiMovementCooldown(view);
      IssueLiveActorCommand(&state, movement.value);
      return;
    case AreaMovementAction::kDirectionalMovement:
      // The original cleared only the first cache byte, 0x1F8003D0.
      if (movement.clear_direction_cache) Frame().walk_valid = 0;
      ResolveLiveDirectionalMovement(&state, movement.value, 0);
      return;
  }
  Require(false, "AI area movement chose no movement");
}
}  // namespace VS::Battle::Ai
