// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:685/1238,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/NavigationPrimitives.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr int kTileZShift = 16;
constexpr unsigned kDirectionComponentMask = 3;
constexpr int kDirectionZShift = 4;
constexpr int kDirectionComponentModulus = 4;
constexpr int kMaximumGroundStep = 64;
constexpr unsigned kOccupancyCellBytes = sizeof(std::uint16_t);
constexpr unsigned kGridSide = kNavigationGridSide;
constexpr unsigned kOccupancyRowBytes = kOccupancyCellBytes * kGridSide;
constexpr unsigned kOccupancyMapBytes = kOccupancyRowBytes * kGridSide;
constexpr std::size_t kRouteEndPairs = kRouteGateEndCount / 2;
constexpr std::uint32_t kFirstPlatformGate = 0x400;
constexpr std::uint32_t kSecondPlatformGate = 0x04000000;
constexpr std::uint32_t kFirstClosedGate = 0x8000;
constexpr std::uint32_t kSecondClosedGate = 0x80000000;
constexpr unsigned kGateSelectorMask = 3;
constexpr unsigned kGateCoordinateMask = 31;
constexpr unsigned kGateCoordinateBits = 5;
constexpr unsigned kSecondGateShift = 16;
constexpr unsigned kGateSelectorShift = 13;
constexpr unsigned kVariableShiftMask = 31;
constexpr unsigned kCoordinateHalfwordBits = 16;
constexpr unsigned kCoordinateByteBits = 8;
constexpr unsigned kCoordinateHalfwordMask = 0xFFFF;
constexpr std::uint32_t kHorizontalTileMask = 0xFF00FF;
constexpr std::uint32_t kScriptedWaitBits = 0xFE0;
constexpr unsigned kScriptedWaitShift = 5;
constexpr unsigned kScriptedWaitMask = 127;
constexpr std::int32_t kScriptedArrivalSquared = 1024;
constexpr unsigned kFineCoordinateShift = 6;
constexpr unsigned kFineCoordinateHighShift = 14;
constexpr unsigned kTileCoordinateShift = 7;
constexpr unsigned kGroundAreaMask = 15;
constexpr std::uint8_t kNoMovementTarget = 16;
constexpr int kBattleActorSlots = 42;

bool InGrid(int x, int z) {
  return x >= 0 && z >= 0 && x < kNavigationGridSide && z < kNavigationGridSide;
}

bool ValidActorId(int actor_id) {
  if (actor_id >= 0 && actor_id < kNavigationActorSlots) return true;
  Core_Warnf("ai", "AI navigation actor ID %d is out of bounds", actor_id);
  return false;
}

bool SetTileFlag(NavigationGrid& grid, int x, int z, std::uint16_t flag) {
  if (!grid.HasTileFlags()) return true;
  if (!InGrid(x, z)) {
    Core_Warnf("ai", "AI navigation tile (%d,%d) is out of bounds", x, z);
    return false;
  }
  grid.SetTileFlags(x, z,
                    static_cast<std::uint16_t>(grid.TileFlags(x, z) | flag));
  return true;
}

// 7E05C.s:57-63 forms a byte offset, including the upper layer bits.
bool ChangeOccupancy(NavigationGrid& grid, TileOccupantPlacement placement,
                     int delta) {
  if (!grid.HasTileFlags()) return true;
  const unsigned offset = (placement.x * kOccupancyCellBytes) +
                          (placement.layer >> 2U) +
                          (placement.z * kOccupancyRowBytes);
  if (!grid.HasOccupancyHeights() ||
      offset > kOccupancyMapBytes - kOccupancyCellBytes ||
      offset % kOccupancyCellBytes != 0) {
    Core_Warnf("ai", "Gimmick occupancy placement (%u,%u,%u) is invalid",
               static_cast<unsigned>(placement.x),
               static_cast<unsigned>(placement.layer),
               static_cast<unsigned>(placement.z));
    return false;
  }
  const auto cell = offset / kOccupancyCellBytes;
  const auto x = static_cast<int>(cell % kNavigationGridSide);
  const auto z = static_cast<int>(cell / kNavigationGridSide);
  const int height = static_cast<int>(grid.OccupancyHeight(x, z)) + delta;
  grid.SetOccupancyHeight(x, z, static_cast<std::uint16_t>(height));
  return true;
}

int SignedDirection(unsigned value) {
  return value > 1 ? static_cast<int>(value) - kDirectionComponentModulus
                   : static_cast<int>(value);
}

std::uint32_t AvailableRouteWord(
    std::uint32_t word, std::span<const RouteGateEnd, kRouteGateEndCount> ends,
    std::uint32_t active) {
  if ((word & (kFirstPlatformGate | kSecondPlatformGate)) == 0) return word;
  word |= kFirstClosedGate | kSecondClosedGate;
  const auto first_x = (word >> kGateCoordinateBits) & kGateCoordinateMask;
  const auto first_z = word & kGateCoordinateMask;
  const auto second_x =
      (word >> (kSecondGateShift + kGateCoordinateBits)) & kGateCoordinateMask;
  const auto second_z = (word >> kSecondGateShift) & kGateCoordinateMask;
  auto first_bit = (word >> kGateSelectorShift) & kGateSelectorMask;
  auto second_bit =
      (word >> (kSecondGateShift + kGateSelectorShift)) & kGateSelectorMask;
  for (std::size_t link_index = 0; link_index < kRouteEndPairs; ++link_index) {
    const auto& end = ends[link_index * 2];
    if ((word & kFirstPlatformGate) != 0 && first_x == end.x) {
      first_bit = 1u << (first_bit & kVariableShiftMask);
      if (first_z == end.z) {
        first_bit &= active;
        if (first_bit != 0) word &= ~kFirstClosedGate;
      }
    }
    // 73C84.s mutates both shift operands across pairs, not just their masks.
    second_bit = 1u << (second_bit & kVariableShiftMask);
    if ((word & kSecondPlatformGate) != 0) {
      second_bit &= active;
      if (second_x == end.x && second_z == end.z && second_bit != 0) {
        word &= ~kSecondClosedGate;
      }
    }
  }
  return word;
}

void PrepareScriptedRoute(ScriptedRoute& route,
                          const RouteModelPlacement& model) {
  route.repeat_wait = 0;
  if (route.movement_wait != 0) --route.movement_wait;
  unsigned wait =
      (route.navigation_flags >> kScriptedWaitShift) & kScriptedWaitMask;
  if (wait != 0) --wait;
  route.navigation_flags = (route.navigation_flags & ~kScriptedWaitBits) |
                           (wait << kScriptedWaitShift);
  route.ignore_floor = 1;
  route.gait = 0;
  route.movement_target = kNoMovementTarget;
  route.is_stuck = 0;
  route.height_filter = 0;
  route.tile =
      model.tile_x |
      (static_cast<unsigned>(model.tile_layer) << kCoordinateByteBits) |
      (static_cast<unsigned>(model.tile_z) << kTileZShift) |
      (static_cast<unsigned>(model.tile_status)
       << (kCoordinateHalfwordBits + kCoordinateByteBits));
  route.x = model.x;
  route.y = model.y;
  route.z = model.z;
  route.position_pad = model.position_pad;
  const unsigned x = static_cast<std::uint16_t>(model.x);
  const unsigned z = static_cast<std::uint16_t>(model.z);
  route.fine_x = static_cast<std::uint8_t>(x >> kFineCoordinateShift);
  route.fine_layer = static_cast<std::uint8_t>(x >> kFineCoordinateHighShift);
  route.fine_z = static_cast<std::uint8_t>(z >> kFineCoordinateShift);
  route.fine_status = static_cast<std::uint8_t>(z >> kFineCoordinateHighShift);
}

void ContinueScriptedRoute(ScriptedRouteHost& host, ScriptedRoute& route) {
  if (route.route_gate != 0) {
    host.MoveTowardDestinationTile(route);
    return;
  }
  const unsigned x = route.scripted_target & kCoordinateHalfwordMask;
  const unsigned z = route.scripted_target >> kCoordinateHalfwordBits;
  route.move_x = static_cast<int>(x);
  route.move_z = static_cast<int>(z);
  route.destination = (x >> kTileCoordinateShift) |
                      ((z >> kTileCoordinateShift) << kCoordinateHalfwordBits);
  host.UpdateNavigationAreaMovement(route);
}

bool ScriptedRouteReached(ScriptedRouteHost& host, ScriptedRoute& route) {
  const unsigned target = route.scripted_target;
  if (route.locomotion_mode == 0) {
    const int x = static_cast<int>(target & kCoordinateHalfwordMask);
    const int z = static_cast<int>(target >> kCoordinateHalfwordBits);
    // SquaredVectorLength keeps each delta's low 16 bits, so the original
    // unsigned halfword reads of the position need no separate cast.
    const unsigned squared =
        Core::Math::SquaredVectorLength(x - route.x, 0, z - route.z);
    if (std::bit_cast<std::int32_t>(squared) < kScriptedArrivalSquared) {
      return true;
    }
    ContinueScriptedRoute(host, route);
    return false;
  }
  if ((route.destination & kHorizontalTileMask) ==
      (route.tile & kHorizontalTileMask)) {
    return true;
  }
  route.destination =
      ((target & kCoordinateHalfwordMask) >> kTileCoordinateShift) |
      ((target >> (kCoordinateHalfwordBits + kTileCoordinateShift))
       << kCoordinateHalfwordBits);
  host.DispatchTileMovement(route);
  return false;
}

// One actor's step of func_800E5308; false refuses the update.
bool UpdateScriptedRoute(ScriptedRouteHost& host, int actor_id) {
  if (!ValidActorId(actor_id)) return false;
  auto route = host.Route(actor_id);
  host.StampBudgetTick();
  if (!route) return true;
  const auto model = host.Placement(actor_id);
  if (!model || route->actor_id != actor_id ||
      !InGrid(model->tile_x, model->tile_z)) {
    Core_Warnf("ai",
               "Scripted route for actor %d requires a model, map, tile "
               "and actor identity",
               actor_id);
    return false;
  }
  PrepareScriptedRoute(*route, *model);
  const auto area = static_cast<std::uint8_t>(
      host.TileFlags(model->tile_x, model->tile_z) & kGroundAreaMask);
  route->ground_area = route->area_exception = area;
  host.StoreRoute(*route);
  host.ClearDirectionCache();
  // The source clears guest pointer tag bits at actor byte3, not native links.
  if (ScriptedRouteReached(host, *route)) host.FinishRoute(*route);
  return true;
}
}  // namespace

// decomp: func_800E4C1C (src/BATTLE/BATTLE.PRG/7BDD4.s:542).
bool ReserveAiDestinationTile(NavigationGrid& grid, int x, int z) {
  return SetTileFlag(grid, x, z, kReservedTileFlag);
}

// decomp: func_800E68EC (src/BATTLE/BATTLE.PRG/7E05C.s:72).
bool MarkAiTileDynamicObstacle(NavigationGrid& grid, int x, int z) {
  return SetTileFlag(grid, x, z, kDynamicObstacleFlag);
}

// decomp: func_800E6898 (src/BATTLE/BATTLE.PRG/7E05C.s:38-69).
bool AddAiTileOccupant(NavigationGrid& grid, TileOccupantPlacement placement) {
  return ChangeOccupancy(grid, placement, kDynamicObstacleFlag);
}

// decomp: func_800E68A0 (src/BATTLE/BATTLE.PRG/7E05C.s:46-69).
bool RemoveAiTileOccupant(NavigationGrid& grid,
                          TileOccupantPlacement placement) {
  return ChangeOccupancy(grid, placement, -kDynamicObstacleFlag);
}

// decomp: func_800E4690 (src/BATTLE/BATTLE.PRG/7BDD4.s:72).
std::optional<bool> IsAdjacentTileStepBlocked(
    Field::TerrainGrid terrain, const NavigationDirectionCodes& directions,
    int x, int z, int direction) {
  if (direction < 0 || direction >= kNavigationDirections || !InGrid(x, z)) {
    Core_Warnf("ai", "AI neighbor step from (%d,%d) toward %d is invalid", x, z,
               direction);
    return std::nullopt;
  }
  const unsigned packed = directions[static_cast<std::size_t>(direction)];
  const auto dx = SignedDirection(packed & kDirectionComponentMask);
  const auto dz =
      SignedDirection((packed >> kDirectionZShift) & kDirectionComponentMask);
  if ((dx == 0 && dz == 0) || dx < -1 || dz < -1) {
    Core_Warnf("ai", "AI neighbor direction %d has invalid delta (%d,%d)",
               direction, dx, dz);
    return std::nullopt;
  }
  const auto next_x = x + dx;
  const auto next_z = z + dz;
  if (!InGrid(next_x, next_z)) return true;
  if (Field::GroundHeightFlagsAtTile(terrain, next_x, next_z) < 0) return true;
  return std::abs(Field::GroundHeightAtTile(terrain, x, z) -
                  Field::GroundHeightAtTile(terrain, next_x, next_z)) >=
         kMaximumGroundStep;
}

// decomp: func_800E6B4C (src/BATTLE/BATTLE.PRG/7E05C.s:257).
// Its allocation held a temporary PS1 stack, not terrain data. The bounded
// 32x32 builder uses the native game fiber's existing 16 MiB stack.
bool BuildAiTerrainHeightsWithWorkspace(SpecialTerrainBuilder& builder) {
  if (!builder.HasNativeMaps()) {
    Core_Warnf("ai", "Special terrain build requires active native maps");
    return false;
  }
  builder.BuildSpecialTerrainHeights();
  return true;
}

std::uint32_t VisiblePlatformMask(
    std::span<const std::int16_t> platform_indices) {
  std::uint32_t mask = 0;
  for (auto index = platform_indices.size(); index-- > 0;) {
    mask |= 1u << (static_cast<unsigned>(platform_indices[index]) &
                   kVariableShiftMask);
  }
  return mask;
}

// decomp: func_800DC638 (src/BATTLE/BATTLE.PRG/73C84.s:149-243).
bool UpdateRouteGateAvailability(
    std::span<std::uint32_t, kRouteWordCount> routes,
    std::span<const RouteGateEnd, kRouteGateEndCount> ends,
    std::uint32_t visible_platforms) {
  // No original valid room has a first-word second-only platform gate.
  if (visible_platforms != 0 &&
      (routes[0] & (kFirstPlatformGate | kSecondPlatformGate)) ==
          kSecondPlatformGate) {
    Core_Warnf("ai",
               "Route availability: first route word depends on an "
               "original stack-register value");
    return false;
  }
  for (auto& word : routes) {
    word = AvailableRouteWord(word, ends, visible_platforms);
  }
  return true;
}

// decomp: func_800E5158 (src/BATTLE/BATTLE.PRG/7C7E0.s:127), $a3 = 0.
std::optional<bool> HasActorNavigationState(
    const NavigationOverrideTable& table, int actor_id) {
  if (!ValidActorId(actor_id)) return std::nullopt;
  return table.HasTileFlags() && table.HasActorState(actor_id);
}

// decomp: func_800E5154 (src/BATTLE/BATTLE.PRG/7C7E0.s:122), shared $a3 = 1.
std::optional<int> ActorNavigationOverrideStatus(
    const NavigationOverrideTable& table, int actor_id) {
  constexpr int kUnavailable = 2;
  const auto present = HasActorNavigationState(table, actor_id);
  if (!present) return std::nullopt;
  if (!*present || !table.HasOverride(actor_id)) return kUnavailable;
  const auto tile = table.OverrideTrackedTile(actor_id);
  if (!tile) {
    Core_Warnf("ai", "AI navigation override for actor %d requires a tile",
               actor_id);
    return std::nullopt;
  }
  return (*tile & kHorizontalTileMask) !=
                 (table.OverrideDestination(actor_id) & kHorizontalTileMask)
             ? 1
             : 0;
}

// decomp: func_800E51C8 (src/BATTLE/BATTLE.PRG/7C7E0.s:159).
bool CopyActorNavigationOverrideState(NavigationOverrideCopy& copy,
                                      int actor_id) {
  if (!ValidActorId(actor_id)) return false;
  if (!copy.HasSource(actor_id) || !copy.HasDestination()) {
    Core_Warnf("ai", "AI navigation copy for actor %d requires both states",
               actor_id);
    return false;
  }
  copy.CopySource(actor_id);
  copy.ApplySettings(NavigationOverrideSettings{
      .actor_id = static_cast<std::uint8_t>(actor_id)});
  return true;
}

// decomp: func_800E527C (src/BATTLE/BATTLE.PRG/7C7E0.s:216-256).
bool SetActorNavigationOverride(NavigationOverrideHost& host, int actor_id,
                                int x, int z) {
  const auto present = HasActorNavigationState(host, actor_id);
  if (!present) return false;
  if (!*present) return true;
  if (!host.MatchesActorAndRoom(actor_id)) {
    Core_Warnf("ai",
               "Scripted navigation: path request requires matching "
               "actor and room state");
    return false;
  }
  const std::uint32_t target =
      static_cast<std::uint32_t>(x) |
      (static_cast<std::uint32_t>(z) << kCoordinateHalfwordBits);
  host.ReleaseEngagement(actor_id);
  auto destination = NavigationOverrideTarget::kActorState;
  if (!host.HasOverride(actor_id)) {
    if (!host.AllocateOverride(actor_id)) {
      Core_Warnf("ai", "Scripted navigation: route allocation failed");
      return false;
    }
    destination = NavigationOverrideTarget::kNewRoute;
  }
  // On repeat entry, the original copies onto the live base, not its old route.
  if (!host.CopyOverrideState(actor_id, destination)) return false;
  host.SetScriptedTarget(actor_id, destination, target);
  return true;
}

// decomp: func_800E5308 (src/BATTLE/BATTLE.PRG/7C7E0.s:260-421).
bool UpdateActorNavigationOverrides(ScriptedRouteHost& host) {
  host.BeginUpdate();
  int visited = 0;
  while (const auto actor = host.NextActor()) {
    if (++visited > kBattleActorSlots) {
      Core_Warnf("ai",
                 "Scripted navigation: actor list is cyclic or exceeds "
                 "native storage");
      return false;
    }
    if (actor->suspended) continue;
    if (!UpdateScriptedRoute(host, actor->id)) return false;
  }
  host.SaveMovementRandomState();
  return true;
}
}  // namespace VS::Battle::Ai
