// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/AreaRoute.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kEndpointCoordinateMask = 31;
constexpr unsigned kEndpointXShift = 5;
constexpr unsigned kHalfwordShift = 16;
constexpr unsigned kTileTagShift = 8;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kTileTagMask = 0xFF00;
constexpr std::uint32_t kTileXZMask = 0xFF00FF;
constexpr std::uint32_t kTileStatusMask = 0xFF000000;
// Route link positions are 1/4096 world units; platform contacts are world
// units; both use 128 world units per tile.
constexpr int kWorldPointTileShift = 19;
constexpr int kContactTileShift = 7;
constexpr int kTileCentre = 64;
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr std::uint16_t kAreaMask = 15;
constexpr std::uint16_t kTerrainLayerBits = 0x1C00;

// Endpoint flags.
constexpr std::uint16_t kEndpointGate = 0x400;
constexpr std::uint16_t kEndpointClosed = 0x8000;
constexpr std::uint16_t kGateClosed = kEndpointGate | kEndpointClosed;
constexpr std::uint16_t kEndpointLongStep = 0x6000;
// An endpoint's x and step kind bits; a first endpoint without any is no
// route, a second one without any is no second endpoint.
constexpr std::uint16_t kEndpointDefined = 0x1BE0;
constexpr unsigned kStepKindShift = 11;
constexpr unsigned kStepKindMask = 3;
constexpr unsigned kStepLengthShift = 13;
constexpr unsigned kStepLengthMask = 3;
// A gate's tag is its platform plus two, also its contact occupancy bit.
constexpr int kGateTagBase = 2;

constexpr unsigned kDirectionComponentMask = 3;
constexpr unsigned kDirectionSignBit = 2;
constexpr int kDirectionComponentModulus = 4;
constexpr unsigned kDirectionZShift = 4;

constexpr std::size_t kMaxPlatformPositions{Field::kMovingPlatformCapacity};
// The original passed the heading in its 4096-per-turn units, scaled by 512.
constexpr unsigned kGroundMovementAngleShift = 9;

std::nullopt_t Refuse(const char* message) {
  Core_Warnf("ai", "%s", message);
  return std::nullopt;
}

int EndpointX(std::uint16_t endpoint) {
  return static_cast<int>((endpoint >> kEndpointXShift) &
                          kEndpointCoordinateMask);
}

int EndpointZ(std::uint16_t endpoint) {
  return static_cast<int>(endpoint & kEndpointCoordinateMask);
}

std::uint32_t EndpointTile(std::uint16_t endpoint) {
  return (static_cast<std::uint32_t>(EndpointZ(endpoint)) << kTileZShift) |
         static_cast<std::uint32_t>(EndpointX(endpoint));
}

bool EndAt(RouteGateEnd end, int x, int z) { return end.x == x && end.z == z; }

bool InGrid(int x, int z) {
  return x >= 0 && z >= 0 && x < kNavigationGridSide && z < kNavigationGridSide;
}

std::uint32_t WithTag(std::uint32_t tile, int tag) {
  return (tile & ~kTileTagMask) |
         ((static_cast<std::uint32_t>(tag) << kTileTagShift) & kTileTagMask);
}

// The original added to each byte, which wraps.
std::uint32_t Stepped(std::uint32_t tile, int dx, int dz) {
  const auto x = static_cast<std::uint8_t>(RouteTileX(tile) + dx);
  const auto z = static_cast<std::uint8_t>(RouteTileZ(tile) + dz);
  return (tile & ~kTileXZMask) |
         (static_cast<std::uint32_t>(z) << kTileZShift) | x;
}

// Each component is a two-bit two's-complement value, -2..1.
int DirectionComponent(unsigned code) {
  const auto value = code & kDirectionComponentMask;
  return (value & kDirectionSignBit) != 0
             ? static_cast<int>(value) - kDirectionComponentModulus
             : static_cast<int>(value);
}

struct RouteStep {
  int direction = 0;
  int dx = 0;
  int dz = 0;
};

std::optional<RouteStep> StepOf(const AreaRouteWorld& world,
                                std::uint16_t endpoint) {
  const int direction = world.RouteStepDirection(
      static_cast<int>((endpoint >> kStepKindShift) & kStepKindMask));
  if (direction >= kNavigationDirections) {
    return Refuse("AI area route step has no direction");
  }
  const unsigned code =
      world.DirectionCodes()[static_cast<std::size_t>(direction)];
  return RouteStep{.direction = direction,
                   .dx = DirectionComponent(code),
                   .dz = DirectionComponent(code >> kDirectionZShift)};
}

// The exit word the original wrote: the exit's three bytes over the old
// fourth, or the whole entry word when no link starts there.
std::uint32_t RouteExitWord(RouteGateEnds ends, std::uint32_t entry,
                            std::uint32_t previous) {
  const auto exit = ResolveRouteGateExit(ends, entry);
  if (!exit) return entry;
  return (previous & kTileStatusMask) |
         (static_cast<std::uint32_t>(exit->z) << kTileZShift) |
         (static_cast<std::uint32_t>(exit->platform) << kTileTagShift) |
         exit->x;
}

// The original's centre label: face the target actor, or else the centre of
// the candidate tile.
std::optional<CommandIntent> TurnToward(const AreaRouteWorld& world,
                                        AreaRouteActor& actor,
                                        std::uint32_t candidate) {
  const int target = actor.movement_target;
  if (target > kAreaRouteNoTarget) {
    return Refuse("AI area route target is outside the actor table");
  }
  if (target != kAreaRouteNoTarget && world.HasActor(target)) {
    const auto position = world.ActorPosition(target);
    if (!position) return Refuse("AI area route target has no position");
    actor.move_x = position->x;
    actor.move_z = position->z;
  } else {
    actor.move_x =
        (static_cast<int>(RouteTileX(candidate)) << kContactTileShift) +
        kTileCentre;
    actor.move_z =
        (static_cast<int>(RouteTileZ(candidate)) << kContactTileShift) +
        kTileCentre;
  }
  actor.angle = static_cast<std::int16_t>(
      static_cast<std::uint32_t>(Core::Math::Arctangent2(
          actor.x - actor.move_x, actor.z - actor.move_z)) &
      kAngleMask);
  actor.turn_timer = actor.turn_period;
  return CommandIntent::kTurn;
}

// The original's accept label.
CommandIntent StepTo(AreaRouteActor& actor, std::uint32_t tile) {
  actor.next_tile = tile;
  return CommandIntent::kStepToTile;
}

// Sets the destination to the endpoint and turns once the actor stands there.
std::optional<CommandIntent> HeadFor(const AreaRouteWorld& world,
                                     AreaRouteActor& actor,
                                     std::uint16_t endpoint,
                                     std::uint32_t candidate) {
  actor.destination = EndpointTile(endpoint);
  if ((actor.destination & kTileXZMask) == (actor.tile & kTileXZMask)) {
    return TurnToward(world, actor, candidate);
  }
  return CommandIntent::kMoveDestination;
}

std::optional<std::uint16_t> HeightAt(const AreaRouteWorld& world, int x,
                                      int z) {
  if (!world.HasTileMaps()) {
    return Refuse("AI area route requires the room's tile maps");
  }
  if (!InGrid(x, z)) {
    return Refuse("AI area route reads a height outside the grid");
  }
  return world.TileHeight(x, z);
}

// An endpoint gate at the actor's tile: board a waiting platform through an
// open gate, or else turn toward the gate tile.
std::optional<CommandIntent> UsePlatformGate(AreaRouteWorld& world,
                                             AreaRouteActor& actor,
                                             std::uint32_t candidate) {
  const auto ends = world.RouteGateEnds();
  if (!ends) return Refuse("AI area route requires the room's route links");
  const auto positions = world.PlatformPositions();
  const auto link = FindRouteGateAtTile(*ends, positions, actor.tile, true);
  if (!link) return std::nullopt;
  if (*link >= 0) {
    const int platform = *link >> 1;
    if (world.PlatformDwellTicks(platform)) {
      const int tag = platform + kGateTagBase;
      candidate = WithTag(candidate, tag);
      const auto closed = world.ClosedGates();
      if (!closed) return Refuse("AI area route requires the room AI state");
      if (((*closed >> tag) & 1) == 0) {
        actor.route_gate = static_cast<std::uint8_t>(tag);
        if (actor.actor_id != 0) world.SetContactOccupancy(tag);
        actor.route_exit = RouteExitWord(*ends, candidate, actor.route_exit);
        return StepTo(actor, candidate);
      }
    }
  }
  return TurnToward(world, actor, candidate);
}

// Outcome of the endpoint at the actor's tile; nothing tries the next one.
struct EndpointOutcome {
  bool next = false;
  std::optional<CommandIntent> intent;
};

EndpointOutcome NextEndpoint() {
  return {.next = true, .intent = std::nullopt};
}

EndpointOutcome Decided(std::optional<CommandIntent> intent) {
  return {.next = false, .intent = intent};
}

// A long step: several tiles along the route at once, changing area first.
EndpointOutcome TakeLongStep(AreaRouteWorld& world, AreaRouteActor& actor,
                             std::uint16_t endpoint, std::uint32_t& candidate) {
  const auto flags =
      NavigationTileFlags(world, RouteTileZ(candidate), RouteTileX(candidate));
  if (!flags) return Decided(std::nullopt);
  actor.area = static_cast<std::uint8_t>(*flags & kAreaMask);
  actor.target_distance_filter = 0;
  const auto step = StepOf(world, endpoint);
  if (!step) return Decided(std::nullopt);
  const int length =
      static_cast<int>((endpoint >> kStepLengthShift) & kStepLengthMask) + 1;
  candidate = Stepped(candidate, length * step->dx, length * step->dz);
  const auto occupancy = world.DestinationOccupancy(actor, candidate);
  if (!occupancy) {
    return Decided(Refuse("AI area route occupancy query refused"));
  }
  if (*occupancy < 0) return Decided(TurnToward(world, actor, candidate));
  if (*occupancy == 0) return NextEndpoint();
  return Decided(StepTo(actor, WithTag(candidate, 0)));
}

// A single step along the route, walking unless the next tile changes layer
// or the step leaves the tile's region.
std::optional<CommandIntent> TakeStep(AreaRouteWorld& world,
                                      AreaRouteActor& actor,
                                      std::uint16_t endpoint,
                                      std::uint32_t candidate) {
  const auto step = StepOf(world, endpoint);
  if (!step) return std::nullopt;
  candidate = Stepped(candidate, step->dx, step->dz);
  const auto occupancy = world.DestinationOccupancy(actor, candidate);
  if (!occupancy) return Refuse("AI area route occupancy query refused");
  if (*occupancy <= 0) return TurnToward(world, actor, candidate);
  const auto height =
      HeightAt(world, RouteTileX(candidate), RouteTileZ(candidate));
  if (!height) return std::nullopt;
  if ((*height & kTerrainLayerBits) != 0) {
    return StepTo(actor, WithTag(candidate, 0));
  }
  const auto cell =
      world.TerrainRegionCell(RouteTileX(actor.tile), RouteTileZ(actor.tile));
  if (!cell) return Refuse("AI area route tile has no terrain region");
  if (((*cell >> step->direction) & 1) != 0) {
    return StepTo(actor, WithTag(candidate, 0));
  }
  actor.heading = static_cast<std::int16_t>(step->direction);
  return CommandIntent::kMoveDirection;
}

EndpointOutcome FollowEndpoint(AreaRouteWorld& world, AreaRouteActor& actor,
                               std::uint16_t endpoint,
                               std::uint32_t& candidate) {
  if ((endpoint & kEndpointGate) != 0) {
    if ((endpoint & kEndpointClosed) != 0) return NextEndpoint();
    return Decided(UsePlatformGate(world, actor, candidate));
  }
  if ((endpoint & kEndpointLongStep) != 0) {
    return TakeLongStep(world, actor, endpoint, candidate);
  }
  return Decided(TakeStep(world, actor, endpoint, candidate));
}

// With one endpoint on a raised layer, head for the other unless it is raised
// too; then, as with a single endpoint, keep the tile's area.
std::optional<std::optional<CommandIntent>> AvoidRaisedEndpoint(
    const AreaRouteWorld& world, AreaRouteActor& actor,
    const AreaRouteEndpoints& endpoints, int count) {
  for (int index = 0; index < count; ++index) {
    const auto endpoint = endpoints.at(static_cast<std::size_t>(index));
    const auto height =
        HeightAt(world, EndpointX(endpoint), EndpointZ(endpoint));
    if (!height) return std::optional<CommandIntent>{};
    if ((*height & kTerrainLayerBits) == 0) continue;
    const auto other = endpoints.at(static_cast<std::size_t>(1 - index));
    const auto other_height =
        HeightAt(world, EndpointX(other), EndpointZ(other));
    if (!other_height) return std::optional<CommandIntent>{};
    if ((*other_height & kTerrainLayerBits) != 0 || count == 1) {
      return SetNavigationAreaFromTile(world, actor, actor.tile);
    }
    actor.destination = EndpointTile(other);
    return CommandIntent::kMoveDestination;
  }
  return std::nullopt;
}

int SquaredDistance(int x, int z, std::uint16_t endpoint) {
  const int dx = x - EndpointX(endpoint);
  const int dz = z - EndpointZ(endpoint);
  return (dx * dx) + (dz * dz);
}
}  // namespace

// decomp: func_800DF9A8 (src/BATTLE/BATTLE.PRG/771A8.c:459).
std::optional<RouteGateExit> ResolveRouteGateExit(RouteGateEnds ends,
                                                  std::uint32_t entry_tile) {
  const int x = RouteTileX(entry_tile);
  const int z = RouteTileZ(entry_tile);
  for (std::size_t link = 0; link < kRouteGateLinkCount; ++link) {
    if (!EndAt(ends[link * 2], x, z)) continue;
    const auto partner = link ^ 1U;
    const auto end = ends[(partner * 2) + 1];
    return RouteGateExit{.x = end.x,
                         .platform = static_cast<std::uint8_t>(partner >> 1),
                         .z = end.z};
  }
  return std::nullopt;
}

// decomp: func_800DFA54 (src/BATTLE/BATTLE.PRG/771A8.c:480).
std::optional<RouteGateEnd> FindNearestRouteGateEntry(RouteGateEnds ends,
                                                      int platform,
                                                      std::int32_t world_x,
                                                      std::int32_t world_z) {
  if (platform < 0 || platform >= kRouteGatePlatformCount) {
    return Refuse("Route gate entry platform is outside the link table");
  }
  const int x = world_x >> kWorldPointTileShift;
  const int z = world_z >> kWorldPointTileShift;
  const auto first = static_cast<std::size_t>(platform) * 2;
  auto link = first;
  for (auto candidate = first; candidate < first + 2; ++candidate) {
    if (EndAt(ends[(candidate * 2) + 1], x, z)) {
      link = candidate;
      break;
    }
  }
  return ends[link * 2];
}

// decomp: func_800DFAF8 (src/BATTLE/BATTLE.PRG/771A8.c:498).
std::optional<int> FindRouteGateAtTile(
    RouteGateEnds ends, std::span<const Field::PlatformContact> positions,
    std::uint32_t tile, bool match_entry_end) {
  if (positions.size() > kMaxPlatformPositions) {
    return Refuse("Route gate lookup has more platforms than the original");
  }
  const int x = RouteTileX(tile);
  const int z = RouteTileZ(tile);
  const std::size_t matched_end = match_entry_end ? 0 : 1;
  for (std::size_t link = 0; link < kRouteGateLinkCount; ++link) {
    if (!EndAt(ends[(link * 2) + matched_end], x, z)) continue;
    const auto report = link >> 1;
    if (report >= positions.size()) continue;
    const auto& position = positions[report];
    if (EndAt(ends[(link * 2) + 1], position.x >> kContactTileShift,
              position.z >> kContactTileShift)) {
      return static_cast<int>(link);
    }
  }
  return -1;
}

// decomp: routeWord (src/BATTLE/BATTLE.PRG/771A8.c:572).
std::uint32_t RouteEndpointPair(const AreaRouteEndpoints& endpoints) {
  return endpoints[0] |
         (static_cast<std::uint32_t>(endpoints[1]) << kHalfwordShift);
}

// decomp: routeHalf (src/BATTLE/BATTLE.PRG/771A8.c:574).
std::optional<std::uint16_t> RouteEndpointValue(
    const AreaRouteEndpoints& endpoints, int index) {
  if (index < 0 || index >= static_cast<int>(endpoints.size())) {
    return Refuse("Route endpoint index is outside the pair");
  }
  return endpoints.at(static_cast<std::size_t>(index));
}

// decomp: routeTile (src/BATTLE/BATTLE.PRG/771A8.c:576).
std::optional<std::uint16_t> NavigationTileFlags(const AreaRouteWorld& world,
                                                 int z, int x) {
  if (!world.HasTileMaps()) {
    return Refuse("AI area route requires the room's tile maps");
  }
  if (!InGrid(x, z)) {
    return Refuse("AI area route reads tile flags outside the grid");
  }
  return world.TileFlags(x, z);
}

// decomp: routeSetArea (src/BATTLE/BATTLE.PRG/771A8.c:581).
std::optional<CommandIntent> SetNavigationAreaFromTile(
    const AreaRouteWorld& world, AreaRouteActor& actor, std::uint32_t tile) {
  const auto flags =
      NavigationTileFlags(world, RouteTileZ(tile), RouteTileX(tile));
  if (!flags) return std::nullopt;
  actor.area = static_cast<std::uint8_t>(*flags & kAreaMask);
  return CommandIntent::kMoveDestination;
}

// decomp: func_800DFBCC (src/BATTLE/BATTLE.PRG/771A8.c:587).
std::optional<CommandIntent> ResolveNavigationAreaRoute(AreaRouteWorld& world,
                                                        AreaRouteActor& actor,
                                                        int index, int area) {
  if (index < 0 || index >= kAreaRouteNeighbours || area < 0 ||
      area >= kAreaRouteNeighbours) {
    return Refuse("AI area route is outside the room's route table");
  }
  const auto route = world.Route((area * kAreaRouteNeighbours) + index);
  if (!route) return Refuse("AI area route requires the room's routes");
  const auto& endpoints = *route;
  const auto routes = RouteEndpointPair(endpoints);
  if ((routes & kEndpointDefined) == 0) return CommandIntent::kMoveDestination;
  actor.area = static_cast<std::uint8_t>(area);
  const int count =
      (routes & (std::uint32_t{kEndpointDefined} << kHalfwordShift)) != 0 ? 2
                                                                          : 1;

  // An endpoint at the actor's tile moves the actor along the route.
  const auto position = actor.tile;
  auto candidate = position;
  for (int index_in_pair = 0; index_in_pair < count; ++index_in_pair) {
    const auto endpoint = endpoints.at(static_cast<std::size_t>(index_in_pair));
    if (EndpointX(endpoint) != RouteTileX(position) ||
        EndpointZ(endpoint) != RouteTileZ(position)) {
      continue;
    }
    candidate = position;
    const auto outcome = FollowEndpoint(world, actor, endpoint, candidate);
    if (!outcome.next) return outcome.intent;
  }

  // A closed gate rules its endpoint out.
  const bool first_closed = (endpoints[0] & kGateClosed) == kGateClosed;
  const bool second_closed = (endpoints[1] & kGateClosed) == kGateClosed;
  if (first_closed) {
    if (count == 1 || second_closed) {
      return SetNavigationAreaFromTile(world, actor, position);
    }
    return HeadFor(world, actor, endpoints[1], candidate);
  }
  if (second_closed) return HeadFor(world, actor, endpoints[0], candidate);

  if (const auto avoided =
          AvoidRaisedEndpoint(world, actor, endpoints, count)) {
    return *avoided;
  }
  if (count == 1) return HeadFor(world, actor, endpoints[0], candidate);
  const int x = RouteTileX(position);
  const int z = RouteTileZ(position);
  const bool first_nearer =
      SquaredDistance(x, z, endpoints[0]) < SquaredDistance(x, z, endpoints[1]);
  return HeadFor(world, actor, first_nearer ? endpoints[0] : endpoints[1],
                 candidate);
}

namespace {
std::optional<int> TileArea(const AreaRouteWorld& world, std::uint32_t tile) {
  const auto flags =
      NavigationTileFlags(world, RouteTileZ(tile), RouteTileX(tile));
  if (!flags) return std::nullopt;
  return *flags & kAreaMask;
}

int TileCentreOf(std::uint8_t tile) {
  return (static_cast<int>(tile) << kContactTileShift) + kTileCentre;
}

// The original's 32-bit subtraction, which wraps.
std::int32_t WrappingDelta(std::int32_t from, std::int32_t to) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(from) -
                                   static_cast<std::uint32_t>(to));
}

// Where the area route leaves an actor whose destination lies in another
// area: handed to another movement primitive, or else stuck or not.
struct DestinationRoute {
  std::optional<AreaMovementStep> handoff;
  bool stuck = false;
};

std::optional<DestinationRoute> RouteToDestinationArea(AreaRouteWorld& world,
                                                       AreaRouteActor& actor,
                                                       int current, int next) {
  const auto intent = ResolveNavigationAreaRoute(world, actor, current, next);
  if (!intent) return std::nullopt;
  if (*intent == CommandIntent::kMoveDirection) {
    return DestinationRoute{
        .handoff = AreaMovementStep{
            .action = AreaMovementAction::kGroundMovement,
            .value = static_cast<int>(static_cast<std::uint32_t>(actor.heading)
                                      << kGroundMovementAngleShift)}};
  }
  if (*intent != CommandIntent::kMoveDestination) {
    return DestinationRoute{
        .handoff = AreaMovementStep{.action = AreaMovementAction::kCommand,
                                    .value = static_cast<int>(*intent)}};
  }
  actor.move_x = TileCentreOf(RouteTileX(actor.destination));
  actor.move_z = TileCentreOf(RouteTileZ(actor.destination));
  const auto destination_area = TileArea(world, actor.destination);
  if (!destination_area) return std::nullopt;
  const auto tile_area = TileArea(world, actor.tile);
  if (!tile_area) return std::nullopt;
  return DestinationRoute{.handoff = std::nullopt,
                          .stuck = *destination_area != *tile_area};
}
}  // namespace

// decomp: func_800E19FC (src/BATTLE/BATTLE.PRG/771A8.c:1540).
std::optional<AreaMovementStep> UpdateNavigationAreaMovement(
    AreaRouteWorld& world, AreaMovementActor& actor) {
  actor.near_target_detour = 0;
  actor.route.target_distance_filter = 1;
  if (actor.height_filtered_movement != 0) {
    return AreaMovementStep{.action =
                                AreaMovementAction::kHeightFilteredDirections};
  }
  auto& route = actor.route;
  const auto current = TileArea(world, route.tile);
  if (!current) return std::nullopt;
  const auto next = TileArea(world, route.destination);
  if (!next) return std::nullopt;

  bool stuck = false;
  if (*current != *next) {
    stuck = true;
    if (actor.area_lock != 0) {
      const auto routed = RouteToDestinationArea(world, route, *current, *next);
      if (!routed) return std::nullopt;
      if (routed->handoff) return routed->handoff;
      stuck = routed->stuck;
    }
    if (stuck) actor.is_stuck = 1;
  }
  const auto angle = static_cast<std::uint32_t>(Core::Math::Arctangent2(
                         WrappingDelta(route.x, route.move_x),
                         WrappingDelta(route.z, route.move_z))) &
                     kAngleMask;
  return AreaMovementStep{.action = AreaMovementAction::kDirectionalMovement,
                          .value = static_cast<int>(angle),
                          .clear_direction_cache = stuck};
}
}  // namespace VS::Battle::Ai
