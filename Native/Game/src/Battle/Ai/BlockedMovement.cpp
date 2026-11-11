// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/BlockedMovement.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr std::uint32_t kTileByteMask = 0xFF;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kTileXZMask = 0xFF00FF;
constexpr unsigned kStatusShift = 24;
constexpr std::uint32_t kFoundStatus = 1;
constexpr std::uint32_t kBlockedStatus = 2;
// Everything below the status byte.
constexpr std::uint32_t kBelowStatusMask = 0xFFFFFF;
// Clears the tag byte, bits 8-15.
constexpr std::uint32_t kResultMask = 0xFFFF00FF;
constexpr std::uint32_t kDirectionArgumentMask = 0xFFFF;

constexpr std::uint8_t kFullHistory = 0xFF;

constexpr int kTileShift = 7;
constexpr int kTileCentre = 64;
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr unsigned kDirectionAngleShift = 9;
constexpr unsigned kLateralTurn = 2;
constexpr unsigned kHalfTurn = 4;
constexpr unsigned kDirectionMask = 7;
constexpr int kOrderRange = 2;
constexpr std::uint8_t kExhaustedStep = 5;
constexpr std::size_t kSecondOpposite = 3;

void Warn(const char* message) { Core_Warnf("ai", "%s", message); }

int TileX(std::uint32_t tile) { return static_cast<int>(tile & kTileByteMask); }

int TileZ(std::uint32_t tile) {
  return static_cast<int>((tile >> kTileZShift) & kTileByteMask);
}

// `base` with the x and z bytes of the tile's neighbour one step away, each
// wrapped to a byte as the original stored them.
std::uint32_t Neighbour(std::uint32_t base, std::uint32_t tile,
                        NavigationStep step) {
  return (base & ~kTileXZMask) |
         (static_cast<std::uint32_t>(
              static_cast<std::uint8_t>(TileZ(tile) + step.dz))
          << kTileZShift) |
         static_cast<std::uint8_t>(TileX(tile) + step.dx);
}

std::uint32_t Blocked(std::uint32_t destination) {
  return ((destination & kBelowStatusMask) | kBlockedStatus << kStatusShift) &
         kResultMask;
}

int TileCentre(int tile) { return (tile << kTileShift) + kTileCentre; }

std::uint8_t Turned(std::uint32_t direction, unsigned turn) {
  return static_cast<std::uint8_t>((direction + turn) & kDirectionMask);
}

// The angle's 45-degree sector between the actor and its destination's tile
// centre and the direction two past it, the sector first only when it is the
// angle's nearest axis direction, then their opposites in a random order.
DetourDirections ChooseDetourDirections(BlockedMovementWorld& world,
                                        const BlockedMover& mover) {
  const auto angle = static_cast<std::uint32_t>(Core::Math::Arctangent2(
                         mover.x - TileCentre(TileX(mover.destination)),
                         mover.z - TileCentre(TileZ(mover.destination)))) &
                     kAngleMask;
  const auto sector = angle >> kDirectionAngleShift;
  const auto first = Turned(sector, 0);
  const auto second = Turned(sector, kLateralTurn);
  DetourDirections directions{second, first, 0, 0};
  if (NearestAxisDirection(angle) == first) directions = {first, second, 0, 0};
  const bool first_opposite_first = world.NextRandomMasked(kOrderRange) != 0;
  directions[2] = Turned(first_opposite_first ? first : second, kHalfTurn);
  directions[kSecondOpposite] =
      Turned(first_opposite_first ? second : first, kHalfTurn);
  return directions;
}

// Stores the detour directions and sets the step to the first whose
// neighbour is free, plus one; 1 when none is. False when the world refuses.
bool StartDetour(BlockedMovementWorld& world, BlockedMover& mover,
                 const NavigationDirectionCodes& codes) {
  const auto directions = ChooseDetourDirections(world, mover);
  mover.detour_directions = directions;
  for (std::size_t index = 0; index < directions.size(); ++index) {
    const auto probe =
        Neighbour(0, mover.tile, DecodeDirectionCode(codes[directions[index]]));
    const auto occupancy = world.DestinationOccupancy(mover, probe);
    if (!occupancy.has_value()) return false;
    if (*occupancy > 0) {
      mover.detour_step = static_cast<std::uint8_t>(index);
      break;
    }
  }
  ++mover.detour_step;
  return true;
}
}  // namespace

std::optional<std::uint32_t> FindAdjacentMovementDestination(
    AdjacentDestinationWorld& world, const AdjacentMover& mover, int first,
    int second, std::uint32_t direction) {
  if (mover.movement_disabled) return 0U;
  std::array<std::uint32_t, 2> directions{direction & kDirectionArgumentMask,
                                          0};
  std::size_t count = 1;
  if (first != second) {
    const int nearest = NearestAxisDirection(direction);
    directions = {
        static_cast<std::uint32_t>(nearest),
        static_cast<std::uint32_t>(nearest != first ? first : second) &
            kDirectionArgumentMask};
    count = 2;
  }
  const auto codes = world.DirectionCodes();
  // The original's register: status 1 until a gap landing replaces it.
  std::uint32_t destination = kFoundStatus << kStatusShift;
  for (std::size_t index = 0; index < count; ++index) {
    const auto probe = directions[index];
    if (probe >= kDirectionCodeCount) {
      Warn("AI adjacent destination direction is outside 0..7");
      return std::nullopt;
    }
    const auto step_direction = static_cast<int>(probe);
    destination =
        Neighbour(destination, mover.tile, DecodeDirectionCode(codes[probe]));
    const auto edge =
        world.CanTraverseTileEdge(mover.tile, destination, step_direction);
    if (!edge.has_value()) return std::nullopt;
    if (*edge > 0) {
      const auto occupancy = world.DestinationOccupancy(destination);
      if (!occupancy.has_value()) return std::nullopt;
      if (*occupancy < 0) return Blocked(destination);
      return *occupancy != 0 ? destination & kResultMask : 0U;
    }
    if (*edge < 0) return Blocked(destination);
    const auto landing =
        world.FindGapLandingTile(destination, step_direction, mover.tile);
    if (!landing.has_value()) return std::nullopt;
    destination = *landing;
    const auto status = destination >> kStatusShift;
    if (status == kFoundStatus) return destination & kResultMask;
    if (status != 0) return Blocked(destination);
  }
  return 0U;
}

bool HasRepeatingMovementHistory(const MovementHistory& history) {
  return history.x_words[0] == history.x_words[1] &&
         history.z_words[0] == history.z_words[1];
}

bool IsMovementHistoryStalled(const MovementHistory& history) {
  return history.mask == kFullHistory && HasRepeatingMovementHistory(history);
}

void SaveMovementRandomState(MovementRandomStates& states) {
  states.saved = static_cast<std::int32_t>(states.frame);
}

std::optional<bool> TryRecoverBlockedMovement(BlockedMovementWorld& world,
                                              BlockedMover& mover) {
  if (world.HasArenaOrbit()) return false;
  if (((mover.tile ^ mover.previous_tile) & kTileXZMask) != 0) {
    mover.detour_step = 0;
    return false;
  }
  mover.is_stuck = 1;
  const auto codes = world.DirectionCodes();
  if (mover.detour_step == 0 && !StartDetour(world, mover, codes)) {
    return std::nullopt;
  }
  if (mover.detour_step >= kExhaustedStep) {
    world.ForgetActionTarget(mover);
    mover.holds_stall_tile = 0;
    mover.detour_step = 0;
    return false;
  }
  // A step the world wrapped to 0 would read the byte before the directions.
  if (mover.detour_step == 0 ||
      mover.detour_directions[mover.detour_step - 1U] >= kDirectionCodeCount) {
    Warn("AI detour direction is outside 0..7");
    return std::nullopt;
  }
  const auto direction = mover.detour_directions[mover.detour_step - 1U];
  mover.destination = Neighbour(mover.destination, mover.tile,
                                DecodeDirectionCode(codes[direction]));
  mover.movement_target = mover.detour_target;
  const auto occupancy = world.DestinationOccupancy(mover, mover.destination);
  if (!occupancy.has_value()) return std::nullopt;
  if (*occupancy > 0) {
    world.DispatchLocomotion(mover);
  } else {
    ++mover.detour_step;
  }
  return true;
}
}  // namespace VS::Battle::Ai
