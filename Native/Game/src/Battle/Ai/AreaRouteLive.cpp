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
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Legacy/Psx.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Battle::Ai {
namespace {
using RouteView = VS::Battle::Ai::AreaRouteView;
using AiState = VS::Battle::Ai::ActorAiState;

constexpr std::size_t kTileWordBytes = sizeof(std::uint32_t);
// A gate tile's x, platform and z bytes.
constexpr std::size_t kRouteGateTileBytes = 3;
constexpr unsigned kHalfwordShift = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;

// The routing view aliases the actor AI state it is handed.
static_assert(offsetof(RouteView, opaque5) == offsetof(AiState, opaque5));
static_assert(offsetof(RouteView, tile) == offsetof(AiState, tile));
static_assert(offsetof(RouteView, x) == offsetof(AiState, position));
static_assert(offsetof(RouteView, native_pointer60) ==
              offsetof(AiState, native_pointer60));
static_assert(offsetof(RouteView, actor_id) == offsetof(AiState, actor_id));
static_assert(offsetof(RouteView, cooldown) == offsetof(AiState, opaque_13a));
static_assert(offsetof(RouteView, period) == offsetof(AiState, opaque_13c));
static_assert(offsetof(RouteView, area) == offsetof(AiState, reserved154_area));
static_assert(offsetof(RouteView, movement_target) ==
              offsetof(AiState, movement_target));
static_assert(offsetof(RouteView, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(RouteView, next_tile) == offsetof(AiState, next_tile));
static_assert(offsetof(RouteView, move_x) == offsetof(AiState, move_x));
static_assert(offsetof(RouteView, move_z) == offsetof(AiState, move_z));
static_assert(offsetof(RouteView, heading) == offsetof(AiState, heading));
static_assert(offsetof(RouteView, gate) == offsetof(AiState, route_gate));
static_assert(offsetof(RouteView, exit) == offsetof(AiState, route_exit_bytes));
static_assert(sizeof(AiState{}.route_exit_bytes) == kTileWordBytes);
static_assert(sizeof(VS::Battle::Ai::TilePosition) == kTileWordBytes);
static_assert(kFrameActorSlots == kAreaRouteNoTarget);
static_assert(sizeof(VS::Battle::Ai::RouteGateTile) == kRouteGateTileBytes);
// So does the area movement view.
using MoveView = VS::Battle::Ai::TerrainMoveView;
static_assert(offsetof(MoveView, is_stuck) == offsetof(AiState, is_stuck));
static_assert(offsetof(MoveView, opaque4) == offsetof(AiState, opaque4));
static_assert(offsetof(MoveView, opaque5) == offsetof(AiState, opaque5));
static_assert(offsetof(MoveView, opaque_1a) == offsetof(AiState, opaque_1a));
static_assert(offsetof(MoveView, settings_bit14) ==
              offsetof(AiState, settings_bit14));
static_assert(offsetof(MoveView, tile) == offsetof(AiState, tile));
static_assert(offsetof(MoveView, position) == offsetof(AiState, position));
static_assert(offsetof(MoveView, destination) ==
              offsetof(AiState, destination));
static_assert(offsetof(MoveView, move_x) == offsetof(AiState, move_x));
static_assert(offsetof(MoveView, move_z) == offsetof(AiState, move_z));
static_assert(offsetof(MoveView, angle) == offsetof(AiState, heading));
static_assert(offsetof(MoveView, actor) == offsetof(AiState, actor));

AiState& State(void* view) {
  Require(view != nullptr, "AI area routing requires an AI state");
  return *static_cast<AiState*>(view);
}

FrameContext& Frame() { return LiveFrameContext(); }

std::optional<std::array<RouteGateEnd, kRouteGateEndCount>> LiveGateEnds() {
  if (VS::Battle::Ai::routeGates == nullptr) return std::nullopt;
  return DecodeRouteGateEnds(
      std::span<const RouteGateEndRecord, kRouteGateEndCount>(
          VS::Battle::Ai::routeGates, kRouteGateEndCount));
}

AreaRouteActor Load(const AiState& state) {
  std::uint32_t exit = 0;
  std::memcpy(&exit, state.route_exit_bytes.data(), sizeof(exit));
  return {.target_distance_filter = state.opaque5,
          .tile = state.tile.word,
          .x = state.position.vx,
          .z = state.position.vz,
          .actor_id = state.actor_id,
          .turn_timer = state.opaque_13a,
          .turn_period = state.opaque_13c,
          .area = state.reserved154_area,
          .movement_target = state.movement_target,
          .destination = state.destination.word,
          .next_tile = state.next_tile.word,
          .move_x = state.move_x,
          .move_z = state.move_z,
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
  state.actor_id = actor.actor_id;
  state.opaque_13a = actor.turn_timer;
  state.opaque_13c = actor.turn_period;
  state.reserved154_area = actor.area;
  state.movement_target = actor.movement_target;
  state.destination.word = actor.destination;
  state.next_tile.word = actor.next_tile;
  state.move_x = actor.move_x;
  state.move_z = actor.move_z;
  state.heading = actor.heading;
  state.angle = actor.angle;
  state.route_gate = actor.route_gate;
  std::memcpy(state.route_exit_bytes.data(), &actor.route_exit,
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
    const auto word = static_cast<const std::uint32_t*>(
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
    return LiveTerrainRegionCell(x, z);
  }
  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] std::uint8_t RouteStepDirection(int kind) const override {
    return VS::Battle::Ai::routeStepDirections[static_cast<std::size_t>(kind)];
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
    if (VS::Battle::Ai::roomAiState == nullptr) return std::nullopt;
    return VS::Battle::Ai::roomAiState->reserved_e_prefix[0];
  }
  void SetContactOccupancy(int bit) override { SetLiveAiContactOccupancy(bit); }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      const AreaRouteActor& actor, std::uint32_t tile) override {
    Store(actor, state_);
    return LiveDestinationOccupancy(
        reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&state_), tile);
  }
  [[nodiscard]] bool HasActor(int slot) const override {
    return Frame().actors[slot] != nullptr;
  }
  [[nodiscard]] std::optional<AreaRoutePoint> ActorPosition(
      int slot) const override {
    const auto* position =
        static_cast<const SVECTOR*>(Frame().actors[slot]->native_pointer60);
    if (position == nullptr) return std::nullopt;
    return AreaRoutePoint{.x = position->vx, .z = position->vz};
  }

 private:
  AiState& state_;
};

// The room's terrain regions and the actor's directional movement.
class LiveHeightFilteredMoveWorld final : public HeightFilteredMoveWorld {
 public:
  explicit LiveHeightFilteredMoveWorld(AiState& state) : state_(state) {}

  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const override {
    return LiveTerrainRegionCell(x, z);
  }
  void SetTerrainRegionCell(int x, int z, std::uint8_t value) override {
    std::uint8_t* cell = Frame().terrainRegions != nullptr
                             ? FindTerrainRegionCell(x, z)
                             : nullptr;
    Require(cell != nullptr, "AI height filter writes outside the regions");
    *cell = value;
  }
  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }
  [[nodiscard]] std::optional<DirectionalMoveOutcome> MoveInDirection(
      int angle) override {
    ResolveLiveDirectionalMovement(&state_, angle, 0);
    return DirectionalMoveOutcome{
        .movement_result = Frame().movementResult,
        .heading = static_cast<std::uint16_t>(state_.heading)};
  }

 private:
  AiState& state_;
};

std::uint32_t TileWord(const VS::Battle::Ai::RouteGateTile* tile) {
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

void ResolveLiveRouteGateExit(VS::Battle::Ai::RouteGateTile* entry_tile,
                              VS::Battle::Ai::RouteGateTile* exit_tile) {
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
                                   VS::Battle::Ai::RouteGateTile* entry_tile) {
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

int ResolveLiveNavigationAreaRoute(VS::Battle::Ai::AreaRouteView* view,
                                   int index, int area) {
  auto& state = State(view);
  LiveAreaRouteWorld world(state);
  auto actor = Load(state);
  const auto intent = ResolveNavigationAreaRoute(world, actor, index, area);
  Store(actor, state);
  return static_cast<int>(
      Required(intent, "AI area route refused its room or actor"));
}

void UpdateLiveNavigationAreaMovement(VS::Battle::Ai::TerrainMoveView* view) {
  auto& state = State(view);
  LiveAreaRouteWorld world(state);
  AreaMovementActor actor{.route = Load(state),
                          .is_stuck = state.is_stuck,
                          .near_target_detour = state.opaque4,
                          .height_filtered_movement = state.opaque_1a,
                          .area_lock = state.settings_bit14};
  const auto step = UpdateNavigationAreaMovement(world, actor);
  Store(actor.route, state);
  state.is_stuck = actor.is_stuck;
  state.opaque4 = actor.near_target_detour;
  const auto movement =
      Required(step, "AI area movement refused its room or actor");
  switch (movement.action) {
    case AreaMovementAction::kHeightFilteredDirections:
      MoveLiveWithHeightFilteredDirections(view);
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

unsigned char FilterLiveDirectionsByGroundHeight(unsigned char mask,
                                                 unsigned int tile,
                                                 int maximum_height) {
  return Required(
      FilterDirectionsByGroundHeight(mask, tile, maximum_height,
                                     LiveDirectionCodes(), CurrentTerrain()),
      "AI height filter refused its terrain");
}

void MoveLiveWithHeightFilteredDirections(
    VS::Battle::Ai::TerrainMoveView* view) {
  auto& state = State(view);
  // The original named the actor before the move, which may replace it.
  auto* actor = view->actor;
  LiveHeightFilteredMoveWorld world(state);
  const auto move = Required(
      MoveWithHeightFilteredDirections(world, {.tile = state.tile.word,
                                               .x = state.position.vx,
                                               .y = state.position.vy,
                                               .z = state.position.vz,
                                               .move_x = state.move_x,
                                               .move_z = state.move_z}),
      "AI height-filtered move refused its room or movement");
  if (!move.walk_heading) return;
  Require(actor != nullptr, "AI height-filtered walk requires an actor");
  actor->command_opcode = static_cast<std::uint8_t>(CommandOpcode::kWalk);
  actor->command_heading = *move.walk_heading;
}
}  // namespace VS::Battle::Ai
