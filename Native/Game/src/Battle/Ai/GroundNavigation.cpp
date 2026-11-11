// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_movement.c:702-960,1717-1744,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/GroundNavigation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kByteMask = 0xFF;
constexpr std::uint32_t kTileMask = 0xFF00FF;
constexpr std::uint32_t kLowTileMask = 0xFFFFFF;
constexpr int kTileShift = 7;
constexpr int kTileCenter = 64;
constexpr unsigned kTileZShift = 16;
constexpr unsigned kByteShift = 8;
constexpr unsigned kStatusShift = 24;
constexpr unsigned kTerrainLayer = 0x1C00;
constexpr unsigned kGapFlag = 0x20;
constexpr unsigned kCellBlocked = 0x80B0;
constexpr unsigned kNoStepFlags = 0x30;
constexpr unsigned kCellWalkBlocked = 0xB0;
constexpr unsigned kCellFloorBlocked = 0x10;
constexpr unsigned kExtendedStep = 0xF8;
constexpr int kExtendedStepHeight = 0x2000;
constexpr int kOccupancyHeight = 128;
constexpr int kApproachMargin = 96;
constexpr int kMaximumGapAcceptance = 3;
constexpr unsigned kAngleMask = 0xFFF;
constexpr int kDirectionMask = 7;
constexpr unsigned kAreaMask = 15;
constexpr unsigned kShiftMask = 31;
constexpr std::uint32_t kPathDirectionBits = 0xE;
constexpr int kHalfTurnDirections = 4;
constexpr std::uint32_t kPathVerticalAxisBit = 4;
constexpr int kDirectionalModes = 4;
constexpr int kDetourRandomRange = 4;
constexpr unsigned kPathTurnShift = 4;
constexpr int kInitialHesitationChance = 8;
constexpr int kRouteLinks = 8;
constexpr unsigned kRepeatThreshold = 5;
constexpr std::uint8_t kRepeatWait = 120;
constexpr unsigned kDetourSteps = 126;
constexpr int kRandomRange = 128;
constexpr int kPollKind = 3;
constexpr int kLocomotionModes = 3;
constexpr std::uint16_t kTurnDuration = 64;
constexpr int kPitchHigh = 0x500;
constexpr int kPitchMiddle = 0x320;
constexpr int kSecondCryThreshold = 11;
constexpr int kFirstCryLow = 18;
constexpr int kFirstCryHigh = 53;
constexpr unsigned kHalfwordBits = 16;
constexpr int kTerrainIntent = 11;
constexpr int kAlternateTerrainIntent = 12;
constexpr int kStepIntent = 14;
constexpr int kWalkDirectionIntent = 5;
constexpr int kWalkHeadingIntent = 7;
constexpr int kTimedTurnIntent = 16;
constexpr int kGroundDetourIntent = 11;
constexpr int kFlightDetourIntent = 12;
constexpr std::uint16_t kBlockedWait = 254;
constexpr std::uint32_t kPathCellKeepMask = 0xFFC30FFF;
constexpr std::uint32_t kTileTagMask = 0xFF00;
constexpr unsigned kQuadrantMask = 0x3FF;
constexpr unsigned kQuadrantShift = 10;
constexpr unsigned kCardinalShift = 9;
constexpr unsigned kCardinalMask = 6;
constexpr int kDiagonalAngleShift = 9;
constexpr int kAlignedCosine = 3959;
constexpr int kFacingBase = 0xC00;
constexpr unsigned kDirectionZShift = 4;
constexpr unsigned kComponentMask = 3;
constexpr int kComponentModulus = 4;
constexpr int kNeighborRowStride = 4;
constexpr int kActorSlots = 16;
constexpr int kGridSide = kNavigationGridSide;
constexpr std::uint32_t kPathHeadingShift = 1;
constexpr std::uint32_t kPathStepMask = 0xFE0;
constexpr unsigned kPathStepShift = 5;
constexpr std::uint32_t kPathLowMask = 0x3FFFFF;
constexpr unsigned kPathXShift = 22;
constexpr unsigned kPathZShift = 27;
constexpr std::uint32_t kPathTurn = 0x10;
constexpr std::uint32_t kPathStop = 0x10000;
constexpr std::uint32_t kPathChanged = 0x20000;
constexpr unsigned kPathCellShift = 18;
constexpr unsigned kNearTargetSquared = 0x31000;
constexpr int kModeSlots = 7;
constexpr std::size_t kModeStride = 2;
constexpr std::uint32_t kHighBit = 0x80000000U;
constexpr std::array<int, 12> kNeighbors{1, 0, 7, 0, 2, -1, 6, 0, 3, 4, 5, 0};
constexpr std::array<unsigned, 2> kVerticalBits{0x20, 0x10};
constexpr std::array<int, 2> kVerticalCodes{10, 9};

std::int16_t Half(std::uint32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

int Angle(int value) {
  return static_cast<int>(static_cast<unsigned>(value) & kAngleMask);
}

struct Tile {
  int x;
  int z;
};

Tile Unpack(std::uint32_t tile) {
  return {static_cast<int>(tile & kByteMask),
          static_cast<int>((tile >> kTileZShift) & kByteMask)};
}

std::uint32_t Pack(Tile tile) {
  return static_cast<std::uint32_t>(tile.x) |
         (static_cast<std::uint32_t>(tile.z) << kTileZShift);
}

int TileCentre(int tile) { return (tile * (1 << kTileShift)) + kTileCenter; }

struct RoomGrid {
  int width;
  int height;
};

// The original's tile bound check; the room must fit the 32x32 maps.
std::optional<RoomGrid> ActiveGrid(const GroundNavigationWorld& world) {
  const auto bounds = world.RoomBounds();
  const auto width = static_cast<int>(bounds & kByteMask);
  const auto height = static_cast<int>((bounds >> kByteShift) & kByteMask);
  if (width > 0 && height > 0 && width <= kGridSide && height <= kGridSide &&
      world.HasTileMaps() && !world.Terrain().Empty()) {
    return RoomGrid{width, height};
  }
  Core_Warnf("ai", "AI ground navigation requires a bounded room grid (%08X)",
             static_cast<unsigned>(bounds));
  return std::nullopt;
}

bool Contains(RoomGrid grid, Tile tile) {
  return tile.x >= 0 && tile.z >= 0 && tile.x < grid.width &&
         tile.z < grid.height;
}

int Component(unsigned value) {
  value &= kComponentMask;
  return value > 1 ? static_cast<int>(value) - kComponentModulus
                   : static_cast<int>(value);
}

std::optional<Tile> Delta(const GroundNavigationWorld& world, int direction) {
  if (direction < 0 || direction >= kNavigationDirections) {
    Core_Warnf("ai", "AI ground direction %d is out of bounds", direction);
    return std::nullopt;
  }
  const unsigned packed =
      world.DirectionCodes()[static_cast<std::size_t>(direction)];
  const Tile delta{Component(packed), Component(packed >> kDirectionZShift)};
  if ((delta.x == 0 && delta.z == 0) || delta.x < -1 || delta.z < -1) {
    Core_Warnf("ai", "AI ground direction %d has an invalid delta %02X",
               direction, packed);
    return std::nullopt;
  }
  return delta;
}

std::optional<bool> RegionBlocked(const GroundNavigationWorld& world,
                                  const GroundNavigator& navigator,
                                  int direction) {
  if (direction < 0) return true;
  const auto tile = Unpack(navigator.tile);
  if (!world.HasRoomData()) {
    return IsAdjacentTileStepBlocked(world.Terrain(), world.DirectionCodes(),
                                     tile.x, tile.z, direction);
  }
  const auto cell = direction < kNavigationDirections
                        ? world.TerrainRegionCell(tile.x, tile.z)
                        : std::nullopt;
  if (!cell.has_value()) {
    Core_Warnf("ai", "AI region query at (%d,%d) direction %d is invalid",
               tile.x, tile.z, direction);
    return std::nullopt;
  }
  return ((static_cast<unsigned>(*cell) >> static_cast<unsigned>(direction)) &
          1U) != 0;
}

struct Bounds {
  int x;
  int y;
  int z;
  int radius;
  int reach;
  int height;
};

// An actor that is moving collides at its planned position (the player,
// actor 0, never plans one).
Bounds BoundsOf(int actor_id, const GroundCollisionActor& actor) {
  const bool planned =
      actor.has_ai_state && actor_id != 0 && actor.model_moving;
  const auto x = planned ? actor.planned_x : actor.model_x;
  const auto y = planned ? actor.planned_y : actor.model_y;
  const auto z = planned ? actor.planned_z : actor.model_z;
  return {static_cast<std::uint16_t>(x),
          y,
          static_cast<std::uint16_t>(z),
          actor.has_ai_state ? actor.collision_radius : actor.model_radius,
          actor.has_ai_state ? actor.reach : 0,
          actor.has_ai_state ? static_cast<std::uint16_t>(actor.body_height)
                             : actor.model_height};
}

// One more than the first other actor the test accepts, or 0.
template <typename Test>
std::optional<int> FirstBlockingActor(GroundNavigationWorld& world,
                                      const GroundNavigator& navigator,
                                      Test test) {
  world.RewindCollisionActors();
  unsigned visited = 0;
  for (auto id = world.NextCollisionActor(); id.has_value();
       id = world.NextCollisionActor()) {
    if (*id < 0 || *id >= kActorSlots) {
      Core_Warnf("ai", "AI collision actor ID %d is out of bounds", *id);
      return std::nullopt;
    }
    const unsigned bit = 1U << static_cast<unsigned>(*id);
    if ((visited & bit) != 0) {
      Core_Warnf("ai", "AI collision actor list repeats actor %d", *id);
      return std::nullopt;
    }
    visited |= bit;
    if (*id == navigator.actor_id) continue;
    const auto actor = world.CollisionActor(*id);
    if (!actor.has_value()) {
      Core_Warnf("ai", "AI collision actor %d has no model", *id);
      return std::nullopt;
    }
    if (test(BoundsOf(*id, *actor))) return *id + 1;
  }
  return 0;
}

int NeighborDirection(Tile from, Tile to) {
  const int index =
      ((to.z - from.z + 1) * kNeighborRowStride) + to.x - from.x + 1;
  return index >= 0 && static_cast<std::size_t>(index) < kNeighbors.size()
             ? kNeighbors[static_cast<std::size_t>(index)]
             : -1;
}

std::optional<int> ProbeBlocker(GroundNavigationWorld& world,
                                const GroundNavigator& navigator, Tile delta) {
  const int x =
      static_cast<std::uint16_t>(navigator.x) + (navigator.max_range * delta.x);
  const int z =
      static_cast<std::uint16_t>(navigator.z) + (navigator.max_range * delta.z);
  const Tile point{x >> kTileShift, z >> kTileShift};
  const auto grid = ActiveGrid(world);
  if (!grid.has_value()) return std::nullopt;
  if (!Contains(*grid, point)) return 0;
  const auto region = RegionBlocked(
      world, navigator, NeighborDirection(Unpack(navigator.tile), point));
  if (!region.has_value()) return std::nullopt;
  const int height =
      *region ? navigator.y
              : Field::GroundHeightAtWorldPosition(world.Terrain(), x, z);
  return FirstBlockingActor(world, navigator, [&](const Bounds& other) {
    const int dx = other.x - x;
    const int dz = other.z - z;
    if ((delta.x > 0 && dx < 0) || (delta.x < 0 && dx > 0) ||
        (delta.z > 0 && dz < 0) || (delta.z < 0 && dz > 0)) {
      return false;
    }
    const int reach = other.radius + other.reach + navigator.collision_radius;
    return std::abs(dx) <= reach && std::abs(dz) <= reach &&
           std::abs(other.y - height) <=
               (other.height +
                static_cast<std::uint16_t>(navigator.body_height)) /
                   2;
  });
}

// Whether the tile is inside the approach plan's keep-out radius around its
// target.
std::optional<bool> InsideApproachRadius(const GroundNavigationWorld& world,
                                         const GroundNavigator& navigator,
                                         Tile target) {
  const auto position = navigator.approach_target_id < kActorSlots
                            ? world.ModelPosition(navigator.approach_target_id)
                            : std::nullopt;
  if (!position.has_value()) {
    Core_Warnf("ai", "AI approach edge query target %u has no model",
               static_cast<unsigned>(navigator.approach_target_id));
    return std::nullopt;
  }
  const auto dx = static_cast<std::int64_t>(TileCentre(target.x) - position->x);
  const auto dz = static_cast<std::int64_t>(TileCentre(target.z) - position->z);
  const auto radius =
      static_cast<std::int64_t>(navigator.approach_range) - kApproachMargin;
  return (dx * dx) + (dz * dz) < radius * radius;
}

}  // namespace

// decomp: func_800D98E8 (src/BATTLE/BATTLE.PRG/70D4C.s:279).
std::optional<int> DirectionalBlockingActor(GroundNavigationWorld& world,
                                            const GroundNavigator& navigator,
                                            int direction) {
  const auto delta = Delta(world, direction);
  if (!delta.has_value()) return std::nullopt;
  const auto bit = static_cast<std::uint8_t>(1U << direction);
  if ((world.BlockerValid() & bit) != 0) return world.BlockerActor(direction);
  const auto result = ProbeBlocker(world, navigator, *delta);
  if (!result.has_value()) return std::nullopt;
  world.SetBlockerActor(direction, static_cast<std::uint8_t>(*result));
  world.SetBlockerValid(static_cast<std::uint8_t>(world.BlockerValid() | bit));
  return result;
}

// decomp: func_800D954C (src/BATTLE/BATTLE.PRG/70D4C.s:12).
std::optional<int> DestinationOccupancy(GroundNavigationWorld& world,
                                        const GroundNavigator& navigator,
                                        std::uint32_t packed) {
  if ((static_cast<std::uint32_t>(navigator.previous_tile) & kLowTileMask) ==
      (packed & kTileMask)) {
    return 0;
  }
  const auto tile = Unpack(packed);
  const auto grid = ActiveGrid(world);
  if (!grid.has_value()) return std::nullopt;
  if (!Contains(*grid, tile)) return 0;
  const unsigned blocked =
      kCellBlocked |
      (static_cast<unsigned>(navigator.terrain_reject_mask) << kByteShift);
  if ((world.TileFlags(tile.x, tile.z) & blocked) != 0) return 0;
  const int x = TileCentre(tile.x);
  const int z = TileCentre(tile.z);
  const int height = Field::GroundHeightAtTile(world.Terrain(), tile.x, tile.z);
  const auto found =
      FirstBlockingActor(world, navigator, [&](const Bounds& other) {
        const int reach = other.radius + navigator.collision_radius;
        return std::abs(other.x - x) <= reach &&
               std::abs(other.z - z) <= reach &&
               std::abs(other.y - height) <= kOccupancyHeight;
      });
  if (!found.has_value()) return std::nullopt;
  return *found != 0 ? -1 : 1;
}

// decomp: func_800D96C8 (src/BATTLE/BATTLE.PRG/70D4C.s:123).
std::optional<int> CanTraverseTileEdge(GroundNavigationWorld& world,
                                       const GroundNavigator& navigator,
                                       std::uint32_t from, std::uint32_t to,
                                       int direction) {
  if (!Delta(world, direction).has_value()) return std::nullopt;
  const auto source = Unpack(from);
  const auto target = Unpack(to);
  const auto result = [&](int value, const char* reason) {
    Core_Debugf("ai.edge",
                "actor=%u from=%08X to=%08X direction=%d result=%d reason=%s "
                "flags=%04X area=%u stuck=%u height_filter=%u",
                static_cast<unsigned>(navigator.actor_id),
                static_cast<unsigned>(from), static_cast<unsigned>(to),
                direction, value, reason,
                static_cast<unsigned>(navigator.movement_flags),
                static_cast<unsigned>(navigator.ground_area),
                static_cast<unsigned>(navigator.is_stuck),
                static_cast<unsigned>(navigator.height_filter));
    return std::optional<int>(value);
  };
  if (navigator.movement_disabled != 0) return result(0, "disabled-or-bounds");
  const auto grid = ActiveGrid(world);
  if (!grid.has_value()) return std::nullopt;
  if (!Contains(*grid, source) || !Contains(*grid, target)) {
    return result(0, "disabled-or-bounds");
  }
  const unsigned cell = world.TileFlags(target.x, target.z);
  if ((cell & kNoStepFlags) != 0 ||
      (navigator.is_stuck == 0 &&
       (cell & kAreaMask) != navigator.ground_area)) {
    return result(0, "cell-flags-or-area");
  }
  const int dx = target.x - source.x;
  int dz = target.z - source.z;
  // The original negates dz again when dx is negative; kept as found.
  if (dx < 0) dz = -dz;
  if (dz < 0) dz = -dz;
  const auto terrain = world.Terrain();
  const int rise = Field::GroundHeightAtTile(terrain, target.x, target.z) -
                   Field::GroundHeightAtTile(terrain, source.x, source.z);
  bool checked = false;
  if (dx < 2 && dz < 2) {
    const auto blocker = DirectionalBlockingActor(world, navigator, direction);
    if (!blocker.has_value()) return std::nullopt;
    if (*blocker != 0) return result(0, "actor-blocker");
    checked = !(navigator.height_filter != 0 || rise < 1);
  }
  if (!checked) {
    if (navigator.tile_step_limit < dx + dz) return result(0, "step-distance");
    const int limit = (navigator.movement_flags & kExtendedStep) != 0
                          ? kExtendedStepHeight
                          : navigator.step_height;
    if ((navigator.height_filter != 0 && rise > limit) || rise < -limit) {
      return result(0, "step-height");
    }
  }
  if (navigator.target_distance_filter == 0) {
    return result(1, "no-target-distance-filter");
  }
  if (navigator.requested_movement_mode == 1) {
    const auto inside = InsideApproachRadius(world, navigator, target);
    if (!inside.has_value()) return std::nullopt;
    if (*inside) return result(-1, "target-approach-radius");
  }
  const auto destination =
      Core::Math::UnpackTileCoordinate(navigator.destination);
  return result(Core::Math::SquaredTileDistanceFromComponents(
                    target.x, target.z, destination) <
                        Core::Math::SquaredTileDistance(
                            Core::Math::UnpackTileCoordinate(from), destination)
                    ? 1
                    : 0,
                "destination-distance");
}

namespace {
// Whether the fine step from the current tile into the next is blocked.
std::optional<bool> FineStepBlocked(GroundNavigationWorld& world,
                                    const GroundNavigator& navigator,
                                    int direction, Tile next) {
  const auto grid = ActiveGrid(world);
  if (!grid.has_value()) return std::nullopt;
  if (!Contains(*grid, next)) return true;
  const Tile current = Unpack(navigator.tile);
  const auto terrain = world.Terrain();
  if (!terrain.Contains(current.x, current.z)) {
    Core_Warnf("ai", "AI walk probe tile (%d,%d) is outside the maps",
               current.x, current.z);
    return std::nullopt;
  }
  int probe = direction;
  if ((static_cast<unsigned>(direction) & 1U) != 0) {
    probe = NeighborDirection(current, next);
    if (probe < 0) {
      Core_Warnf("ai", "AI fine step (%d,%d) leaves its neighborhood", next.x,
                 next.z);
      return std::nullopt;
    }
  }
  const auto region = RegionBlocked(world, navigator, probe);
  if (!region.has_value()) return std::nullopt;
  bool blocked = *region;
  if (world.HasRoomData() &&
      (static_cast<std::uint32_t>(navigator.previous_tile) & kTileMask) ==
          Pack(next)) {
    blocked = true;
  }
  const unsigned flags = world.TileFlags(next.x, next.z);
  blocked |=
      (navigator.ignore_floor == 0 && (flags & kCellFloorBlocked) != 0) ||
      (flags & (kCellWalkBlocked & ~kCellFloorBlocked)) != 0 ||
      (flags & (static_cast<unsigned>(navigator.terrain_reject_mask)
                << kByteShift)) != 0;
  const unsigned area = world.TileFlags(current.x, current.z) & kAreaMask;
  if (navigator.area_lock != 0 && (navigator.is_stuck & 1U) == 0 &&
      area != (flags & kAreaMask) &&
      (navigator.area_exception & kAreaMask) != (flags & kAreaMask)) {
    blocked = true;
  }
  blocked |= (terrain.Cell(current.x, current.z) & kTerrainLayer) <
             (terrain.Cell(next.x, next.z) & kTerrainLayer);
  return blocked;
}

class WalkProbe final : public GroundDirectionProbe {
 public:
  explicit WalkProbe(GroundNavigationWorld& world) : world_(&world) {}
  [[nodiscard]] std::optional<int> Probe(GroundNavigator& navigator,
                                         int direction) override {
    const auto open = CanWalkInDirection(*world_, navigator, direction);
    if (!open.has_value()) return std::nullopt;
    return *open ? 1 : 0;
  }

 private:
  GroundNavigationWorld* world_;
};

class FlightProbe final : public GroundDirectionProbe {
 public:
  explicit FlightProbe(GroundNavigationWorld& world) : world_(&world) {}
  [[nodiscard]] std::optional<int> Probe(GroundNavigator& navigator,
                                         int direction) override {
    return world_->CanFlyInDirection(navigator, direction);
  }

 private:
  GroundNavigationWorld* world_;
};

std::optional<bool> Open(GroundDirectionProbe& probe,
                         GroundNavigator& navigator, int direction) {
  const auto result = probe.Probe(navigator, direction);
  if (!result.has_value()) return std::nullopt;
  return *result != 0;
}

std::uint32_t NextBoundary(int tile, int target) {
  if (tile == target) return 0;
  return static_cast<std::uint32_t>(tile + (tile < target ? 1 : 0));
}

// The boundary SetDetourBoundary stores counts from the target, not the tile.
std::uint32_t NextTargetBoundary(int tile, int target) {
  if (tile == target) return 0;
  return static_cast<std::uint32_t>(target + (tile < target ? 1 : 0));
}

bool NearTarget(const GroundNavigator& navigator) {
  const auto from = Unpack(navigator.tile);
  const auto to = Unpack(navigator.destination);
  const int dx = (to.x - from.x) * (1 << kTileShift);
  const int dz = (to.z - from.z) * (1 << kTileShift);
  return static_cast<unsigned>((dx * dx) + (dz * dz)) < kNearTargetSquared &&
         (navigator.near_target_detour & 1U) != 0;
}

int FinishStep(GroundNavigationWorld& world, GroundNavigator& navigator,
               std::uint32_t flags, int direction, bool axis_only) {
  const auto plan = RefreshDetourProgress(
      world, navigator,
      (flags & ~kPathDirectionBits) |
          (static_cast<std::uint32_t>(direction) << kPathHeadingShift));
  const auto tile = Unpack(navigator.tile);
  const auto target = Unpack(navigator.destination);
  std::uint32_t x = 0;
  std::uint32_t z = 0;
  if (!axis_only) {
    x = NextBoundary(tile.x, target.x);
    z = NextBoundary(tile.z, target.z);
  } else if ((plan & kPathVerticalAxisBit) != 0) {
    z = static_cast<std::uint32_t>(tile.z + (tile.z < target.z ? 1 : 0));
  } else {
    x = static_cast<std::uint32_t>(tile.x + (tile.x < target.x ? 1 : 0));
  }
  navigator.navigation_flags =
      (plan & kPathLowMask) | (x << kPathXShift) | (z << kPathZShift) | 1U;
  return 1;
}

// A detour planning request: the row picks the turn order (0 both sides
// blocked, 1 one side open, 2 along a cardinal).
struct DetourRequest {
  int row;
  int mode;
  int first;
  int second;
};

// decomp: 7B5DC.s func_800E3EAC / func_800E3FF4 / func_800E40F0.
std::optional<int> PlanStep(GroundNavigationWorld& world,
                            GroundNavigator& navigator,
                            const DetourRequest& request,
                            GroundDirectionProbe& probe) {
  const auto initialized = InitializeObstacleDetour(
      world, navigator, navigator.navigation_flags, probe);
  if (!initialized.has_value()) return std::nullopt;
  const auto flags = *initialized;
  const int turn = (flags & kPathTurn) != 0 ? 1 : -1;
  const int first = request.first;
  const int second = request.second;
  std::array<int, 2> tries{-1, -1};
  int direction = 0;
  if (request.row == 0) {
    const int offset = ((first + (turn * 2)) & kDirectionMask) == second
                           ? turn * 2
                           : kHalfTurnDirections;
    tries[0] = (second + offset) & kDirectionMask;
    direction = (static_cast<int>((flags >> 1) & kDirectionMask) + (turn * 2)) &
                kDirectionMask;
  } else if (request.row == 1) {
    if (turn == second) return FinishStep(world, navigator, flags, first, true);
    tries[0] = (first + kHalfTurnDirections) & kDirectionMask;
    direction = (tries[0] - (turn * 2)) & kDirectionMask;
  } else {
    tries[0] = (first - (turn * 2)) & kDirectionMask;
    tries[1] = (tries[0] + kHalfTurnDirections) & kDirectionMask;
    direction = (first + kHalfTurnDirections) & kDirectionMask;
  }
  for (const int candidate : tries) {
    if (candidate < 0) continue;
    const auto open = Open(probe, navigator, candidate);
    if (!open.has_value()) return std::nullopt;
    if (*open) {
      return FinishStep(world, navigator, flags, candidate, request.row != 0);
    }
  }
  const auto open = Open(probe, navigator, direction);
  if (!open.has_value()) return std::nullopt;
  if (!*open) {
    navigator.navigation_flags = flags;
    return 0;
  }
  return FinishStep(world, navigator, flags, direction, request.row != 0);
}

std::optional<int> Detour(GroundNavigationWorld& world,
                          GroundNavigator& navigator,
                          const DetourRequest& request,
                          GroundDirectionProbe& probe) {
  const bool limited = (static_cast<unsigned>(request.mode) & 1U) != 0;
  if (limited && !NearTarget(navigator)) return 0;
  const auto result = PlanStep(world, navigator, request, probe);
  if (!result.has_value()) return std::nullopt;
  if (limited) SetDetourBoundary(navigator);
  return result;
}

bool DetourOrRecover(GroundNavigationWorld& world, GroundNavigator& navigator,
                     const DetourRequest& request,
                     GroundDirectionProbe& probe) {
  const auto detour = Detour(world, navigator, request, probe);
  if (!detour.has_value()) return false;
  if (*detour == 0) {
    world.Recover(navigator);
  } else {
    navigator.heading = Half((navigator.navigation_flags >> 1) &
                             static_cast<unsigned>(kDirectionMask));
    world.IssueCommand(navigator, kWalkDirectionIntent);
  }
  return true;
}

// Takes an adjacent destination: waits at a blocked one (status 2), turns
// away from one behind the target or steps onto it.
std::optional<bool> AdjacentAction(GroundNavigationWorld& world,
                                   GroundNavigator& navigator,
                                   std::uint32_t tile) {
  if ((tile >> kStatusShift) == 2) {
    if (navigator.movement_wait == 0) navigator.movement_wait = kBlockedWait;
    world.Recover(navigator);
    return true;
  }
  if (tile == 0) return false;
  const auto behind = IsTileBehindActorTarget(world, navigator, tile);
  if (!behind.has_value()) return std::nullopt;
  if (*behind) {
    const auto turned = UpdateHesitationTurn(world, navigator, kRandomRange, 2,
                                             navigator.requested_movement_mode);
    if (!turned.has_value()) return std::nullopt;
    if (*turned != 0) return true;
  }
  navigator.next_tile = tile;
  world.IssueCommand(navigator, kStepIntent);
  return true;
}

// Only once the movement wait has run out (0 or above the detour budget).
std::optional<bool> TryAdjacent(GroundNavigationWorld& world,
                                GroundNavigator& navigator, int first,
                                int second, std::uint32_t angle) {
  if (static_cast<unsigned>(navigator.movement_wait) - 1U < kDetourSteps) {
    return false;
  }
  const auto tile =
      world.FindAdjacentMovementDestination(navigator, first, second, angle);
  return AdjacentAction(world, navigator, tile);
}

void FollowActiveDetour(GroundNavigationWorld& world,
                        GroundNavigator& navigator, bool flight) {
  const int action = world.FollowObstacleDetour(
      navigator, flight, flight ? kFlightDetourIntent : kGroundDetourIntent);
  if (action < 0) {
    world.RefreshMovementCooldown(navigator);
    world.Recover(navigator);
    return;
  }
  navigator.heading = Half((navigator.navigation_flags >> 1) &
                           static_cast<unsigned>(kDirectionMask));
  world.IssueCommand(navigator, action);
}

struct DirectionalMove {
  std::uint32_t angle;
  int angle_argument;
  int mode;
  int nearest;
  Tile current;
  Tile target;
};

bool MoveAlongCardinal(GroundNavigationWorld& world, GroundNavigator& navigator,
                       const DirectionalMove& move,
                       GroundDirectionProbe& probe) {
  const auto direction =
      static_cast<int>((move.angle >> kCardinalShift) & kCardinalMask);
  const auto open = Open(probe, navigator, direction);
  if (!open.has_value()) return false;
  if (*open) {
    navigator.heading = Half(move.angle);
    world.IssueCommand(navigator, kWalkHeadingIntent);
    return true;
  }
  const auto adjacent = TryAdjacent(world, navigator, 0, 0,
                                    static_cast<std::uint32_t>(direction));
  if (!adjacent.has_value()) return false;
  if (*adjacent) return true;
  if (move.mode == 1 && navigator.near_target_detour == 0) {
    world.Recover(navigator);
    return true;
  }
  return DetourOrRecover(world, navigator, {2, move.mode, direction, 0}, probe);
}

bool MoveAcrossQuadrant(GroundNavigationWorld& world,
                        GroundNavigator& navigator, const DirectionalMove& move,
                        GroundDirectionProbe& probe) {
  const unsigned sector = move.angle >> kQuadrantShift;
  const int left = static_cast<int>((sector + 1) * 2) & kDirectionMask;
  const int right = static_cast<int>(sector * 2) & kDirectionMask;
  const int diagonal = static_cast<int>((sector * 2) + 1) & kDirectionMask;
  const auto left_open = Open(probe, navigator, left);
  if (!left_open.has_value()) return false;
  const auto right_open = Open(probe, navigator, right);
  if (!right_open.has_value()) return false;
  if (!*left_open && !*right_open) {
    const auto adjacent =
        TryAdjacent(world, navigator, left, right, move.angle);
    if (!adjacent.has_value()) return false;
    if (*adjacent) return true;
    return DetourOrRecover(world, navigator, {0, move.mode, left, right},
                           probe);
  }
  if (*left_open && *right_open) {
    const auto diagonal_open = Open(probe, navigator, diagonal);
    if (!diagonal_open.has_value()) return false;
    if (!*diagonal_open) {
      navigator.heading =
          Half(static_cast<std::uint32_t>(move.nearest == left ? left : right));
      world.IssueCommand(navigator, kWalkDirectionIntent);
    } else {
      navigator.heading = Half(move.angle);
      world.IssueCommand(navigator, kWalkHeadingIntent);
    }
    return true;
  }
  const int open_side = *right_open ? right : left;
  const int closed_side = *left_open ? right : left;
  navigator.heading = Half(static_cast<std::uint32_t>(open_side));
  const auto adjacent = TryAdjacent(world, navigator, 0, 0,
                                    static_cast<std::uint32_t>(closed_side));
  if (!adjacent.has_value()) return false;
  if (*adjacent) return true;
  const bool aligned = (static_cast<unsigned>(navigator.heading) & 2U) != 0
                           ? move.current.x == move.target.x
                           : move.current.z == move.target.z;
  const bool detour =
      (static_cast<unsigned>(move.mode) & 1U) == 0
          ? aligned
          : navigator.near_target_detour != 0 &&
                Core::Math::Cosine(Angle((closed_side << kDiagonalAngleShift) -
                                         move.angle_argument)) >=
                    kAlignedCosine;
  if (detour) {
    return DetourOrRecover(
        world, navigator,
        {1, move.mode, navigator.heading, *left_open ? 1 : -1}, probe);
  }
  world.IssueCommand(navigator, kWalkDirectionIntent);
  return true;
}

bool ContinueRouteLink(GroundNavigationWorld& world,
                       GroundNavigator& navigator) {
  if (((navigator.tile >> kByteShift) & kByteMask) < 2) {
    navigator.route_exit = ~0U;
    ClearAiContactOccupancy(world, navigator.route_gate);
    navigator.route_gate = 0;
    return true;
  }
  if (!world.HasRouteGates()) {
    Core_Warnf("ai", "AI route continuation requires native route gates");
    return false;
  }
  const int index = world.FindRouteGateAtTile(navigator, navigator.route_exit);
  if (index >= 0) {
    if (index >= kRouteLinks) {
      Core_Warnf("ai", "AI route gate index %d is out of bounds", index);
      return false;
    }
    const auto end = world.RouteLinkStart(index);
    const std::uint32_t tile = Pack({end.x, end.z});
    const auto occupancy = DestinationOccupancy(world, navigator, tile);
    if (!occupancy.has_value()) return false;
    if (*occupancy > 0) {
      navigator.route_exit = ~0U;
      ClearAiContactOccupancy(world, (index >> 1) + 2);
      navigator.route_gate = 0;
      navigator.next_tile = tile;
      world.IssueCommand(navigator, kStepIntent);
      return true;
    }
  }
  world.Recover(navigator);
  return true;
}

std::uint32_t ContactBit(int bit) {
  return 1U << (static_cast<unsigned>(bit) & kShiftMask);
}

// The original ORs a mask whose top bit is clear and ANDs one whose top bit
// is set, so clearing bit 31 sets every lower bit.
void ApplyContactMask(GroundNavigationWorld& world, std::uint32_t mask) {
  if (!world.HasRoomAiState()) return;
  const std::uint32_t value = world.ContactMask();
  const std::uint32_t updated =
      (mask & kHighBit) != 0 ? value & mask : value | mask;
  world.SetContactMask(static_cast<std::uint8_t>(updated));
}

enum class LocomotionRoute {
  kGroundTile,
  kGroundHeading,
  kFlightTile,
  kFlightHeading,
  kRadialTile,
  kRadialHeading
};
}  // namespace

// decomp: D_800D9B6C (src/BATTLE/BATTLE.PRG/70D4C.s:458).
std::optional<bool> CanWalkInDirection(GroundNavigationWorld& world,
                                       const GroundNavigator& navigator,
                                       int direction) {
  const auto delta = Delta(world, direction);
  if (!delta.has_value()) return std::nullopt;
  const auto bit = static_cast<std::uint8_t>(1U << direction);
  if ((world.WalkValid() & bit) != 0) return (world.WalkPassable() & bit) != 0;
  const auto blocker = DirectionalBlockingActor(world, navigator, direction);
  if (!blocker.has_value()) return std::nullopt;
  bool blocked = *blocker != 0;
  const Tile current = Unpack(navigator.tile);
  const Tile next{(navigator.fine_x + delta->x) >> 1,
                  (navigator.fine_z + delta->z) >> 1};
  if (!blocked && (current.x != next.x || current.z != next.z)) {
    const auto fine = FineStepBlocked(world, navigator, direction, next);
    if (!fine.has_value()) return std::nullopt;
    blocked = *fine;
  }
  world.SetWalkValid(static_cast<std::uint8_t>(world.WalkValid() | bit));
  const auto passable = world.WalkPassable();
  world.SetWalkPassable(blocked ? static_cast<std::uint8_t>(passable & ~bit)
                                : static_cast<std::uint8_t>(passable | bit));
  return !blocked;
}

// decomp: func_800DB93C (src/BATTLE/BATTLE.PRG/72B70.s:483).
std::optional<std::uint32_t> FindGapLandingTile(
    GroundNavigationWorld& world, const GroundNavigator& navigator,
    std::uint32_t tile, int direction, std::uint32_t from) {
  const auto delta = Delta(world, direction);
  if (!delta.has_value()) return std::nullopt;
  const auto grid = ActiveGrid(world);
  if (!grid.has_value()) return std::nullopt;
  auto position = Unpack(tile);
  if (!Contains(*grid, position)) return 0U;
  if ((world.TileFlags(position.x, position.z) & kGapFlag) == 0) {
    return tile & kLowTileMask;
  }
  const auto limit =
      std::min<int>(navigator.tile_step_limit, kMaximumGapAcceptance);
  for (int steps = 1; steps <= kGridSide; ++steps) {
    position.x += delta->x;
    position.z += delta->z;
    if (!Contains(*grid, position)) return 0U;
    const auto packed = Pack(position);
    if ((world.TileFlags(position.x, position.z) & kGapFlag) != 0) {
      if (navigator.tile_step_limit != 0) continue;
      return packed;
    }
    int result = 0;
    if (steps < limit) {
      const auto edge =
          CanTraverseTileEdge(world, navigator, from, packed, direction);
      if (!edge.has_value()) return std::nullopt;
      result = *edge;
      if (result == 1) {
        const auto occupancy = DestinationOccupancy(world, navigator, packed);
        if (!occupancy.has_value()) return std::nullopt;
        result = *occupancy;
      }
    }
    return packed | (static_cast<std::uint32_t>(result) << kStatusShift);
  }
  Core_Warnf("ai", "AI gap scan exceeded the bounded room grid");
  return std::nullopt;
}

// decomp: func_800E3BC8 (src/BATTLE/BATTLE.PRG/7B3C8.s:11).
void SetDetourBoundary(GroundNavigator& navigator) {
  const auto target = Unpack(navigator.destination);
  const auto tile = Unpack(navigator.tile);
  const auto x = NextTargetBoundary(tile.x, target.x);
  const auto z = NextTargetBoundary(tile.z, target.z);
  navigator.navigation_flags = (navigator.navigation_flags & kPathLowMask) |
                               (x << kPathXShift) | (z << kPathZShift);
}

// decomp: func_800E3CDC (src/BATTLE/BATTLE.PRG/7B3C8.s:90).
std::uint32_t RefreshDetourProgress(GroundNavigationWorld& world,
                                    GroundNavigator& navigator,
                                    std::uint32_t flags) {
  unsigned repeats =
      (navigator.detour_tile & kTileMask) == (navigator.tile & kTileMask)
          ? navigator.detour_repeats + 1U
          : 0U;
  if (repeats >= kRepeatThreshold) {
    navigator.repeat_wait = kRepeatWait;
    repeats = 0;
  }
  navigator.detour_tile = navigator.tile;
  navigator.detour_repeats = static_cast<std::uint8_t>(repeats);
  navigator.movement_wait = 0;
  unsigned steps = kDetourSteps;
  if (navigator.movement_enabled == 0 &&
      world.NextRandomMasked(kDetourRandomRange) == 0) {
    steps >>= 2U;
  }
  return (flags & ~kPathStepMask) | (steps << kPathStepShift);
}

// decomp: func_800E3D5C (src/BATTLE/BATTLE.PRG/7B55C.s:9).
std::optional<int> CardinalPassabilityMask(GroundNavigator& navigator,
                                           GroundDirectionProbe& probe) {
  int result = 0;
  for (int direction = 0; direction < kNavigationDirections; direction += 2) {
    const auto open = probe.Probe(navigator, direction);
    if (!open.has_value()) return std::nullopt;
    result = static_cast<int>((static_cast<unsigned>(result) << 1U) |
                              static_cast<unsigned>(*open));
  }
  return result;
}

// decomp: func_800E3DDC (src/BATTLE/BATTLE.PRG/7B5DC.s:10).
std::optional<std::uint32_t> InitializeObstacleDetour(
    GroundNavigationWorld& world, GroundNavigator& navigator,
    std::uint32_t flags, GroundDirectionProbe& probe) {
  navigator.detour_run = navigator.gait;
  if ((flags & kPathStepMask) == 0 || (flags & kPathStop) != 0 ||
      navigator.near_target_detour != 0) {
    flags &= ~kPathStop;
    flags = (flags & ~kPathTurn) |
            (static_cast<std::uint32_t>(world.NextRandomMasked(2))
             << kPathTurnShift);
    const auto hesitation =
        UpdateHesitationTurn(world, navigator, kInitialHesitationChance, 1,
                             navigator.requested_movement_mode);
    if (!hesitation.has_value()) return std::nullopt;
  }
  if ((flags & ~kPathStepMask) == 0) flags &= ~kPathChanged;
  const auto mask = CardinalPassabilityMask(navigator, probe);
  if (!mask.has_value()) return std::nullopt;
  return (flags & kPathCellKeepMask) |
         (static_cast<std::uint32_t>(*mask) << kPathCellShift);
}

// decomp: func_800E4180 (src/BATTLE/BATTLE.PRG/7B5DC.s).
std::optional<int> UpdateHesitationTurn(GroundNavigationWorld& world,
                                        GroundNavigator& navigator, int chance,
                                        int kind, int slot) {
  if (slot < 0 || slot >= kModeSlots || kind < 1 || kind > kPollKind) {
    Core_Warnf("ai", "AI hesitation slot %d or kind %d is invalid", slot, kind);
    return std::nullopt;
  }
  // Slots 0..5 keep kind and count in the mode counters; slot 6 keeps its
  // count in the pending reaction byte that follows them.
  const auto offset = static_cast<std::size_t>(slot) * kModeStride;
  auto& stored_kind = navigator.mode_counters[offset];
  auto& remaining = slot != kModeSlots - 1 ? navigator.mode_counters[offset + 1]
                                           : navigator.reaction_pending;
  int count = remaining;
  if (count == 0) {
    if (kind == kPollKind || chance < world.NextRandomMasked(kRandomRange)) {
      return 0;
    }
    count = world.HesitationLength(kind);
    if (count <= 0) {
      Core_Warnf("ai", "AI hesitation lengths were not loaded");
      return std::nullopt;
    }
    stored_kind = static_cast<std::uint8_t>(kind);
  }
  remaining = static_cast<std::uint8_t>(count - 1);
  navigator.turn_timer = kTurnDuration;
  if (kind == 2) {
    navigator.heading =
        static_cast<std::int16_t>(count < kSecondCryThreshold ? 0 : kPitchHigh);
  } else if (kind == 1) {
    navigator.heading =
        static_cast<std::int16_t>(count >= kFirstCryHigh  ? kPitchMiddle
                                  : count >= kFirstCryLow ? -kPitchMiddle
                                                          : 0);
  } else {
    return 0;
  }
  world.IssueCommand(navigator, kTimedTurnIntent);
  return count >= 2 ? 1 : 0;
}

// decomp: func_800E4764 (src/BATTLE/BATTLE.PRG/7BDD4.s:145).
std::optional<VerticalMovementStep> SelectActorVerticalMovementStep(
    const GroundNavigator& navigator) {
  std::size_t index = navigator.gait != 0 ? 0 : 1;
  for (int pass = 0; pass < 2; ++pass, index ^= 1U) {
    const auto bit = kVerticalBits[index];
    if ((navigator.locomotion_flags & (bit | (bit << kByteShift))) == 0) {
      continue;
    }
    return VerticalMovementStep{.selected = true,
                                .value = (navigator.locomotion_flags & bit) != 0
                                             ? navigator.walk_step
                                             : navigator.run_step,
                                .code = kVerticalCodes[index]};
  }
  if (navigator.gait != 0) {
    Core_Warnf("ai", "AI running vertical movement has no configured step");
    return std::nullopt;
  }
  return VerticalMovementStep{};
}

// decomp: func_800E47E4 (src/BATTLE/BATTLE.PRG/7BDD4.s:189).
std::optional<bool> IsTileBehindActorTarget(const GroundNavigationWorld& world,
                                            const GroundNavigator& navigator,
                                            std::uint32_t tile) {
  if (navigator.facing_locked != 0) return false;
  const auto own_facing = navigator.actor_id < kActorSlots
                              ? world.ModelFacing(navigator.actor_id)
                              : std::nullopt;
  if (!own_facing.has_value()) {
    Core_Warnf("ai", "AI facing query actor %u has no model",
               static_cast<unsigned>(navigator.actor_id));
    return std::nullopt;
  }
  int facing = *own_facing;
  if (navigator.movement_target < kActorSlots) {
    const auto position = world.ModelPosition(navigator.movement_target);
    if (!position.has_value()) {
      Core_Warnf("ai", "AI facing query target %u has no model",
                 static_cast<unsigned>(navigator.movement_target));
      return std::nullopt;
    }
    facing =
        Core::Math::Arctangent2(static_cast<std::uint16_t>(navigator.x) -
                                    static_cast<std::uint16_t>(position->x),
                                static_cast<std::uint16_t>(navigator.z) -
                                    static_cast<std::uint16_t>(position->z));
  }
  const auto current = Unpack(navigator.tile);
  const auto target = Unpack(tile);
  const int angle = kFacingBase - facing;
  return (Core::Math::Cosine(angle) * (target.x - current.x)) +
             (Core::Math::Sine(angle) * (target.z - current.z)) <
         0;
}

// decomp: func_800E49D8 (src/BATTLE/BATTLE.PRG/7BDD4.s:342).
std::optional<std::uint32_t> FindAiDescentTile(GroundNavigationWorld& world,
                                               const GroundNavigator& navigator,
                                               std::uint32_t tile,
                                               int direction, int steps) {
  if (!world.HasRoomData() || steps == 0) return 0U;
  const auto delta = Delta(world, direction);
  if (!delta.has_value()) return std::nullopt;
  const auto grid = ActiveGrid(world);
  if (!grid.has_value()) return std::nullopt;
  auto position = Unpack(tile);
  if (!Contains(*grid, position)) return 0U;
  const auto terrain = world.Terrain();
  const auto start = Core::Math::UnpackTileCoordinate(tile);
  if (Field::GroundHeightFlagsAtPackedTile(terrain, start) < 0) return 0U;
  const int floor = Field::GroundHeightAtPackedTile(terrain, start);
  const auto current = Core::Math::UnpackTileCoordinate(navigator.tile);
  if (!terrain.Contains(current.x, current.z)) {
    Core_Warnf("ai", "AI descent origin (%u,%u) is outside the maps",
               static_cast<unsigned>(current.x),
               static_cast<unsigned>(current.z));
    return std::nullopt;
  }
  if (Field::GroundHeightAtPackedTile(terrain, current) >= floor) return 0U;
  for (int taken = 0; taken < kGridSide; ++taken) {
    position.x += delta->x;
    position.z += delta->z;
    if (!Contains(*grid, position)) return 0U;
    if (Field::GroundHeightAtTile(terrain, position.x, position.z) < floor) {
      const std::uint32_t target = (tile & ~kTileMask) | Pack(position);
      const auto edge = CanTraverseTileEdge(world, navigator, navigator.tile,
                                            target, direction);
      if (!edge.has_value()) return std::nullopt;
      if (*edge <= 0) return 0U;
      const auto occupancy = DestinationOccupancy(world, navigator, target);
      if (!occupancy.has_value()) return std::nullopt;
      return *occupancy > 0 ? target : 0U;
    }
    if (taken + 1 >= steps - 1) return 0U;
  }
  Core_Warnf("ai", "AI descent scan exceeded the bounded room grid");
  return std::nullopt;
}

// decomp: func_800E4BD8 (src/BATTLE/BATTLE.PRG/7BDD4.s:527).
void ClearAiContactOccupancy(GroundNavigationWorld& world, int bit) {
  const std::uint32_t mask = ~ContactBit(bit);
  ApplyContactMask(world, mask);
}

// decomp: func_800E4BE0 (src/BATTLE/BATTLE.PRG/7BDD4.s:533).
void SetAiContactOccupancy(GroundNavigationWorld& world, int bit) {
  const std::uint32_t mask = ContactBit(bit);
  ApplyContactMask(world, mask);
}

// decomp: func_800E4B10 (src/BATTLE/BATTLE.PRG/7BDD4.s:431).
void BeginAiTerrainMovement(GroundNavigationWorld& world,
                            GroundNavigator& navigator) {
  navigator.stall_counter = 0;
  navigator.next_tile = navigator.tile;
  world.IssueCommand(navigator, kTerrainIntent);
}

// decomp: func_800E4B18 (src/BATTLE/BATTLE.PRG/7BDD4.s:438).
void BeginAiAlternateTerrainMovement(GroundNavigationWorld& world,
                                     GroundNavigator& navigator) {
  navigator.stall_counter = 0;
  navigator.next_tile = navigator.tile;
  world.IssueCommand(navigator, kAlternateTerrainIntent);
}

// decomp: func_800E4B2C (src/BATTLE/BATTLE.PRG/7BDD4.s:449); the continuation
// is L800DF8A4 (src/BATTLE/BATTLE.PRG/766FC.s).
bool MoveActorTowardDestinationTile(GroundNavigationWorld& world,
                                    GroundNavigator& navigator) {
  if (navigator.route_gate != 0) return ContinueRouteLink(world, navigator);
  if ((navigator.destination & kTileMask) == (navigator.tile & kTileMask)) {
    world.Recover(navigator);
    return true;
  }
  const auto target = Unpack(navigator.destination);
  navigator.move_x = TileCentre(target.x);
  navigator.move_z = TileCentre(target.z);
  world.UpdateNavigationAreaMovement(navigator);
  return true;
}

// decomp: func_800E4B70 (src/BATTLE/BATTLE.PRG/7BDD4.s:473).
bool BeginAiGroundMovement(GroundNavigationWorld& world,
                           GroundNavigator& navigator, int angle) {
  if (navigator.height_filtered_movement != 0) {
    world.MoveWithHeightFilteredDirections(navigator);
    return true;
  }
  if (navigator.route_gate != 0) return ContinueRouteLink(world, navigator);
  navigator.is_stuck = 1;
  world.SetWalkValid(0);
  return ResolveDirectionalMovement(world, navigator, Angle(angle), 1);
}

// decomp: func_800E50A0 (src/BATTLE/BATTLE.PRG/7C7E0.s:66).
bool DispatchAiLocomotionMode(GroundNavigationWorld& world,
                              GroundNavigator& navigator, int kind, int angle) {
  if (navigator.locomotion_mode >= kLocomotionModes || kind < 0 || kind > 1) {
    Core_Warnf("ai", "AI locomotion mode %u or request %d is invalid",
               static_cast<unsigned>(navigator.locomotion_mode), kind);
    return false;
  }
  switch (
      static_cast<LocomotionRoute>((navigator.locomotion_mode * 2) + kind)) {
    case LocomotionRoute::kGroundTile:
      return MoveActorTowardDestinationTile(world, navigator);
    case LocomotionRoute::kGroundHeading:
      return BeginAiGroundMovement(world, navigator, angle);
    case LocomotionRoute::kFlightTile:
      world.UpdateFlightMovement(navigator, angle);
      break;
    case LocomotionRoute::kFlightHeading:
      world.UpdateFlightDirectionalMovement(navigator, angle);
      break;
    case LocomotionRoute::kRadialTile:
      world.MoveToRadialDestination(navigator, angle);
      break;
    case LocomotionRoute::kRadialHeading:
      world.MoveAwayFromReference(navigator, angle);
      break;
  }
  return true;
}

// decomp: func_800E4FE0 (src/BATTLE/BATTLE.PRG/7C7E0.s:10).
bool BeginAiMovementTowardPoint(GroundNavigationWorld& world,
                                GroundNavigator& navigator, int kind) {
  navigator.gait = static_cast<std::uint8_t>(kind);
  navigator.toward_point = 1;
  const std::uint32_t packed =
      static_cast<std::uint32_t>(navigator.move_x) |
      (static_cast<std::uint32_t>(navigator.move_z) << kHalfwordBits);
  navigator.target_x = Half(packed);
  navigator.target_z = Half(packed >> kHalfwordBits);
  return DispatchAiLocomotionMode(
      world, navigator, 1,
      Core::Math::Arctangent2(navigator.move_x - navigator.x,
                              navigator.move_z - navigator.z));
}

// decomp: func_800E3600 (src/BATTLE/BATTLE.PRG/771A8.c:2549;
// nonmatchings/771A8/func_800E3600.s). C counterpart: 771A8.c:2266.
bool ResolveDirectionalMovement(GroundNavigationWorld& world,
                                GroundNavigator& navigator, int angle,
                                int mode) {
  if (mode < 0 || mode >= kDirectionalModes) {
    Core_Warnf("ai", "AI directional movement mode %d is invalid", mode);
    return false;
  }
  const bool flight = mode >= 2;
  if ((navigator.navigation_flags & 1U) != 0) {
    FollowActiveDetour(world, navigator, flight);
    return true;
  }
  const auto unsigned_angle = static_cast<std::uint32_t>(angle);
  const int nearest = NearestAxisDirection(unsigned_angle);
  const auto delta = Delta(world, nearest);
  if (!delta.has_value()) return false;
  const DirectionalMove move{unsigned_angle,
                             angle,
                             mode,
                             nearest,
                             Unpack(navigator.tile),
                             Unpack(navigator.destination)};
  const int distance = std::min<int>(
      navigator.max_range, std::abs((static_cast<unsigned>(nearest) & 2U) != 0
                                        ? move.target.x - move.current.x
                                        : move.target.z - move.current.z));
  const Tile point{move.current.x + delta->x, move.current.z + delta->z};
  const auto descent =
      FindAiDescentTile(world, navigator, Pack(point), nearest, distance);
  if (!descent.has_value()) return false;
  if (*descent != 0) {
    navigator.next_tile = *descent & ~kTileTagMask;
    world.IssueCommand(navigator, kStepIntent);
    return true;
  }
  WalkProbe walk(world);
  FlightProbe fly(world);
  GroundDirectionProbe& probe =
      flight ? static_cast<GroundDirectionProbe&>(fly) : walk;
  if ((unsigned_angle & kQuadrantMask) == 0) {
    return MoveAlongCardinal(world, navigator, move, probe);
  }
  return MoveAcrossQuadrant(world, navigator, move, probe);
}
}  // namespace VS::Battle::Ai
