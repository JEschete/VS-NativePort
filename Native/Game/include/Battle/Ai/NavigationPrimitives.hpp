#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/TerrainHeight.hpp"

// Battle AI navigation primitives: tile reservations and gimmick occupancy on
// the room's AI tile maps, platform-gated route availability and scripted
// navigation overrides. The battle runtime owns the maps, routes and actor AI
// state; these routines see them only through the value records and hosts
// below.
namespace VS::Battle::Ai {
inline constexpr int kNavigationGridSide = 32;
inline constexpr int kNavigationActorSlots = 16;
inline constexpr int kNavigationDirections = 8;
inline constexpr std::size_t kDirectionCodeCount = kNavigationDirections;
inline constexpr std::uint16_t kReservedTileFlag = 0x80;
inline constexpr std::uint16_t kDynamicObstacleFlag = 0x400;
inline constexpr std::size_t kRouteWordCount = 256;
inline constexpr std::size_t kRouteGateEndCount = 16;

// The room's AI tile flag map and gimmick occupancy heights, each indexed by
// tile x and z below kNavigationGridSide.
class NavigationGrid {
 public:
  virtual ~NavigationGrid() = default;
  [[nodiscard]] virtual bool HasTileFlags() const = 0;
  [[nodiscard]] virtual std::uint16_t TileFlags(int x, int z) const = 0;
  virtual void SetTileFlags(int x, int z, std::uint16_t flags) = 0;
  [[nodiscard]] virtual bool HasOccupancyHeights() const = 0;
  [[nodiscard]] virtual std::uint16_t OccupancyHeight(int x, int z) const = 0;
  virtual void SetOccupancyHeight(int x, int z, std::uint16_t value) = 0;
};

// A gimmick's tile; the original's byte offset includes the layer's upper
// bits, so only layers below 4 address their own tile.
struct TileOccupantPlacement {
  std::uint8_t x = 0;
  std::uint8_t layer = 0;
  std::uint8_t z = 0;
};

// Packed neighbor steps, one byte per direction: x in bits 0-1 and z in bits
// 4-5, each 3 meaning -1.
using NavigationDirectionCodes = std::array<std::uint8_t, kDirectionCodeCount>;

// One direction code's tile step.
struct NavigationStep {
  int dx = 0;
  int dz = 0;

  friend bool operator==(const NavigationStep&,
                         const NavigationStep&) = default;
};

// Each component is a two-bit two's-complement value, -2..1, as the original
// decoded it (sll/sra pairs over the code byte).
[[nodiscard]] constexpr NavigationStep DecodeDirectionCode(std::uint8_t code) {
  constexpr unsigned kComponentMask = 3;
  constexpr unsigned kSignBit = 2;
  constexpr int kModulus = 4;
  constexpr unsigned kZShift = 4;
  const auto component = [](unsigned value) {
    value &= kComponentMask;
    return (value & kSignBit) != 0 ? static_cast<int>(value) - kModulus
                                   : static_cast<int>(value);
  };
  return {.dx = component(code), .dz = component(code >> kZShift)};
}

// The axis direction (0, 2, 4 or 6) nearest a 12-bit angle: its quadrant, or
// the next one when the angle lies more than 512 units into it. Shared by
// func_800D8400, func_800DB0BC, func_800E2CCC and func_800E3600.
[[nodiscard]] constexpr int NearestAxisDirection(std::uint32_t angle) {
  constexpr std::uint32_t kAngleMask = 0xFFF;
  constexpr unsigned kQuadrantShift = 10;
  constexpr std::uint32_t kQuadrantAngleMask = 0x3FF;
  constexpr std::uint32_t kQuadrantMidpoint = 512;
  constexpr std::uint32_t kQuadrantMask = 3;
  auto quadrant = (angle & kAngleMask) >> kQuadrantShift;
  if ((angle & kQuadrantAngleMask) > kQuadrantMidpoint) {
    quadrant = (quadrant + 1) & kQuadrantMask;
  }
  return static_cast<int>(quadrant) * 2;
}

// The room's walkable tile bounds (the room AI state's first four bytes,
// D_800F58BC), inclusive.
struct RoomTileBounds {
  std::uint8_t min_x = 0;
  std::uint8_t min_z = 0;
  std::uint8_t max_x = 0;
  std::uint8_t max_z = 0;
};

// decomp: func_800E4C1C (src/BATTLE/BATTLE.PRG/7BDD4.s:542).
// Reserves a destination tile; nothing without a flag map. False refuses a
// tile outside the grid.
bool ReserveAiDestinationTile(NavigationGrid& grid, int x, int z);

// decomp: func_800E68EC (src/BATTLE/BATTLE.PRG/7E05C.s:72).
// Marks a tile as a dynamic obstacle; nothing without a flag map. False
// refuses a tile outside the grid.
bool MarkAiTileDynamicObstacle(NavigationGrid& grid, int x, int z);

// decomp: func_800E6898 (src/BATTLE/BATTLE.PRG/7E05C.s:38-69).
// Raises the occupancy height under a gimmick by one dynamic step, wrapping
// at 16 bits; nothing without a flag map. False refuses a missing height map
// or a placement whose byte offset is odd or outside the map.
bool AddAiTileOccupant(NavigationGrid& grid, TileOccupantPlacement placement);

// decomp: func_800E68A0 (src/BATTLE/BATTLE.PRG/7E05C.s:46-69).
// Lowers the occupancy height under a gimmick; refuses as AddAiTileOccupant.
bool RemoveAiTileOccupant(NavigationGrid& grid,
                          TileOccupantPlacement placement);

// decomp: func_800E4690 (src/BATTLE/BATTLE.PRG/7BDD4.s:72).
// Whether stepping from a tile toward a neighbor is blocked: the neighbor is
// outside the grid, has no ground or differs in height by 64 or more. Nothing
// refuses a direction outside 0..7, a direction whose code is not a unit
// step or a tile outside the grid.
[[nodiscard]] std::optional<bool> IsAdjacentTileStepBlocked(
    Field::TerrainGrid terrain, const NavigationDirectionCodes& directions,
    int x, int z, int direction);

// The room builder of per-tile special terrain heights.
class SpecialTerrainBuilder {
 public:
  virtual ~SpecialTerrainBuilder() = default;
  [[nodiscard]] virtual bool HasNativeMaps() const = 0;
  virtual void BuildSpecialTerrainHeights() = 0;
};

// decomp: func_800E6B4C (src/BATTLE/BATTLE.PRG/7E05C.s:257).
// Builds the special terrain heights. The original allocated a temporary PS1
// stack, not terrain data, so no workspace is needed. False refuses a room
// without its tile flag and height maps.
bool BuildAiTerrainHeightsWithWorkspace(SpecialTerrainBuilder& builder);

// One end of a route link (5-bit tile coordinates).
struct RouteGateEnd {
  std::uint8_t x = 0;
  std::uint8_t z = 0;
};

// Bit mask of the visible moving platforms, one bit per platform index modulo
// 32, as the original shifted by the low five bits.
[[nodiscard]] std::uint32_t VisiblePlatformMask(
    std::span<const std::int16_t> platform_indices);

// decomp: func_800DC638 (src/BATTLE/BATTLE.PRG/73C84.s:149-243).
// Closes each platform-gated route and reopens a gate whose platform is
// visible at a link end. False refuses, before any write, a first route word
// gated only by its second platform while a platform is visible: the
// original then read a stale register.
bool UpdateRouteGateAvailability(
    std::span<std::uint32_t, kRouteWordCount> routes,
    std::span<const RouteGateEnd, kRouteGateEndCount> ends,
    std::uint32_t visible_platforms);

// The AI state table and each actor's scripted navigation override.
class NavigationOverrideTable {
 public:
  virtual ~NavigationOverrideTable() = default;
  [[nodiscard]] virtual bool HasTileFlags() const = 0;
  [[nodiscard]] virtual bool HasActorState(int actor_id) const = 0;
  [[nodiscard]] virtual bool HasOverride(int actor_id) const = 0;
  // The tile the override tracks; nothing when it tracks none.
  [[nodiscard]] virtual std::optional<std::uint32_t> OverrideTrackedTile(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::uint32_t OverrideDestination(
      int actor_id) const = 0;
};

// Movement settings a navigation override starts with.
inline constexpr std::uint16_t kOverrideStepHeight = 0x140;
inline constexpr std::uint8_t kOverrideTileStepLimit = 3;
inline constexpr std::uint8_t kOverrideAreaSettings = 3;
struct NavigationOverrideSettings {
  std::uint8_t actor_id = 0;
  std::uint16_t step_height = kOverrideStepHeight;
  std::uint8_t tile_step_limit = kOverrideTileStepLimit;
  std::uint32_t navigation_flags = 0;
  std::uint8_t repeat_wait = 0;
  std::uint8_t area_settings = kOverrideAreaSettings;
  std::int8_t movement_enabled = 1;
};

// Copies an actor's whole AI state to the override's destination.
class NavigationOverrideCopy {
 public:
  virtual ~NavigationOverrideCopy() = default;
  [[nodiscard]] virtual bool HasSource(int actor_id) const = 0;
  [[nodiscard]] virtual bool HasDestination() const = 0;
  virtual void CopySource(int actor_id) = 0;
  virtual void ApplySettings(const NavigationOverrideSettings& settings) = 0;
};

// Where SetActorNavigationOverride copies the actor's state.
enum class NavigationOverrideTarget { kNewRoute, kActorState };

// The actor, its engagement and the override allocator.
class NavigationOverrideHost : public NavigationOverrideTable {
 public:
  // The actor state belongs to the battle actor and the room has AI state.
  [[nodiscard]] virtual bool MatchesActorAndRoom(int actor_id) const = 0;
  // Disengages the actor, resets its awareness marker and unlinks it.
  virtual void ReleaseEngagement(int actor_id) = 0;
  virtual bool AllocateOverride(int actor_id) = 0;
  virtual bool CopyOverrideState(int actor_id,
                                 NavigationOverrideTarget target) = 0;
  virtual void SetScriptedTarget(int actor_id, NavigationOverrideTarget target,
                                 std::uint32_t target_xz) = 0;
};

// decomp: func_800E5158 (src/BATTLE/BATTLE.PRG/7C7E0.s:127), $a3 = 0.
// Whether the actor has AI state while a tile flag map is loaded. Nothing
// refuses an ID outside the AI state table.
[[nodiscard]] std::optional<bool> HasActorNavigationState(
    const NavigationOverrideTable& table, int actor_id);

// decomp: func_800E5154 (src/BATTLE/BATTLE.PRG/7C7E0.s:122), shared $a3 = 1.
// 2 without state or override, 0 once the tracked tile is the destination
// and 1 while moving. Nothing refuses an ID outside the table or an override
// that tracks no tile.
[[nodiscard]] std::optional<int> ActorNavigationOverrideStatus(
    const NavigationOverrideTable& table, int actor_id);

// decomp: func_800E51C8 (src/BATTLE/BATTLE.PRG/7C7E0.s:159).
// Copies the actor's state and applies the override settings. False refuses
// an ID outside the table or a missing source or destination.
bool CopyActorNavigationOverrideState(NavigationOverrideCopy& copy,
                                      int actor_id);

// decomp: func_800E527C (src/BATTLE/BATTLE.PRG/7C7E0.s:216-256).
// Starts or restarts a scripted move to world (x, z); nothing for an actor
// without navigation state. On repeat entry the original copies onto the
// actor's own state, not its existing route. False refuses an ID outside the
// table, a mismatched actor or room, a failed allocation or copy.
bool SetActorNavigationOverride(NavigationOverrideHost& host, int actor_id,
                                int x, int z);

// An actor's scripted route (the override's AI state).
struct ScriptedRoute {
  std::uint8_t actor_id = 0;
  std::uint8_t locomotion_mode = 0;
  std::uint8_t route_gate = 0;
  std::uint16_t movement_wait = 0;
  std::uint32_t navigation_flags = 0;
  std::uint8_t ignore_floor = 0;
  std::uint8_t gait = 0;
  std::uint8_t movement_target = 0;
  std::uint8_t is_stuck = 0;
  std::uint8_t height_filter = 0;
  std::uint8_t repeat_wait = 0;
  std::uint32_t tile = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t position_pad = 0;
  std::uint8_t fine_x = 0;
  std::uint8_t fine_layer = 0;
  std::uint8_t fine_z = 0;
  std::uint8_t fine_status = 0;
  std::uint8_t ground_area = 0;
  std::uint8_t area_exception = 0;
  std::uint32_t scripted_target = 0;
  std::int32_t move_x = 0;
  std::int32_t move_z = 0;
  std::uint32_t destination = 0;
};

// An actor's model placement.
struct RouteModelPlacement {
  std::uint8_t tile_x = 0;
  std::uint8_t tile_layer = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t tile_status = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
  std::int16_t position_pad = 0;
};

// One battle actor in list order.
struct ScriptedRouteActor {
  int id = 0;
  bool suspended = false;
};

// The battle actor list, the AI frame and each actor's scripted route.
// Movement operations take the route record, which the host stores before
// and reloads after the movement code runs.
class ScriptedRouteHost {
 public:
  virtual ~ScriptedRouteHost() = default;
  virtual void BeginUpdate() = 0;
  // The next battle actor; nothing at the end of the list.
  [[nodiscard]] virtual std::optional<ScriptedRouteActor> NextActor() = 0;
  virtual void StampBudgetTick() = 0;
  [[nodiscard]] virtual std::optional<ScriptedRoute> Route(int actor_id) = 0;
  // The model placement; nothing without a model or tile flag map.
  [[nodiscard]] virtual std::optional<RouteModelPlacement> Placement(
      int actor_id) const = 0;
  [[nodiscard]] virtual std::uint16_t TileFlags(int x, int z) const = 0;
  virtual void StoreRoute(const ScriptedRoute& route) = 0;
  virtual void ClearDirectionCache() = 0;
  virtual void MoveTowardDestinationTile(ScriptedRoute& route) = 0;
  virtual void UpdateNavigationAreaMovement(ScriptedRoute& route) = 0;
  virtual void DispatchTileMovement(ScriptedRoute& route) = 0;
  // Issues the stop command and frees the route.
  virtual void FinishRoute(ScriptedRoute& route) = 0;
  virtual void SaveMovementRandomState() = 0;
};

// decomp: func_800E5308 (src/BATTLE/BATTLE.PRG/7C7E0.s:260-421).
// Advances each unsuspended actor's scripted route and finishes it on
// arrival. False refuses, after the actors before it, an ID outside the
// table, a route that does not match its actor, an actor without a model or
// a list longer than its 42 native slots.
bool UpdateActorNavigationOverrides(ScriptedRouteHost& host);
}  // namespace VS::Battle::Ai
