#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/PlatformPlacement.hpp"

// Room collision map rules: tile cell lookup and blocking, the floor and
// ceiling heights the collision section encodes, gimmick and moving platform
// support heights, and the actor collision bypass bits. Heights are world
// units with up negative; a packed height keeps its value in bits 0..14 and
// sets bit 15 when the sample is invalid.
namespace VS::Field {
// The MPD collision section header (_mpdRoomCollisionMapSection, 146C.h:12):
// the grid in tiles, the height origin and the cell count.
struct CollisionMapHeader {
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int16_t height_origin = 0;
  std::int16_t cell_count = 0;
};

// The header of `collision_map`, or nullopt when the section is missing or
// shorter than the header.
[[nodiscard]] std::optional<CollisionMapHeader> ReadCollisionMapHeader(
    std::span<const std::byte> collision_map);

// Collision layers 0 and 1 are the room grid; layer 2 + n is the n-th moving
// platform's single cell.
inline constexpr std::int32_t kPlatformCollisionLayer = 2;
inline constexpr std::int32_t kTileWorldSize = 128;
// A door passability of 5 blocks the whole tile.
inline constexpr std::uint32_t kBlockedPassability = 5;
inline constexpr std::int16_t kMissingFloorHeight = INT16_MIN;
// The word an absent cell reads as: the original dereferences NULL, and PS1
// RAM word 0 holds 3 after the BIOS boot (psx-spx "Garbage Area at Address
// 00000000h"), so its surface kind reads 3 and every flag clear (#288).
inline constexpr std::uint32_t kAbsentCellWord = 3;

// The tile cell words the rules read: the room's cells (tileCells) and the
// cell word of each authored moving platform, in platform order.
struct CollisionCells {
  std::optional<CollisionMapHeader> map;
  std::span<const std::uint32_t> room;
  std::span<const std::uint32_t> platforms;
};

enum class CollisionCellSource : std::uint8_t { kRoom, kPlatform };

// A cell named by its table and index in CollisionCells.
struct CollisionCellRef {
  CollisionCellSource source = CollisionCellSource::kRoom;
  std::size_t index = 0;

  friend bool operator==(const CollisionCellRef&,
                         const CollisionCellRef&) = default;
};

// A cell's door passability (bits 10..12 of its word): 0 open, 1..4 a
// half-tile diagonal, 5 blocked.
[[nodiscard]] std::uint32_t CellPassability(std::uint32_t cell_word);

// The room's cell table (func_8008B6EC).
[[nodiscard]] std::span<const std::uint32_t> RoomCollisionCells(
    const CollisionCells& cells);

// The grid size packed as the original returns it: height << 20 in the high
// bits, width * 16 in the low halfword; 0 without a collision map.
[[nodiscard]] std::int32_t CollisionMapDimensions(
    std::optional<CollisionMapHeader> map);

// The room's camera bounds in world units.
struct CameraBounds {
  std::int16_t min_x = 0;
  std::int16_t min_z = 0;
  std::int16_t max_x = 0;
  std::int16_t max_z = 0;

  friend bool operator==(const CameraBounds&, const CameraBounds&) = default;
};

// The camera bounds section's record, or nullopt when the room has none (or
// it is shorter than one record).
[[nodiscard]] std::optional<CameraBounds> RoomCameraBounds(
    std::span<const std::byte> zone_bounds);

// The cell at a tile of layer 0 or 1 (both read the first grid, as the
// original does) or the cell of the platform on `layer` >= 2; nullopt off the
// grid, past the cell table, or without a room cell table.
[[nodiscard]] std::optional<CollisionCellRef> CollisionCellAt(
    const CollisionCells& cells, std::uint32_t tile_x, std::uint32_t tile_z,
    std::int32_t layer);

// The cell SetCollisionCellBlocked changes: the tile of grid `layer` (layer
// grids follow each other), or the platform cell of `layer` >= 2. nullopt
// (nothing changes) without a room cell table or outside the tables.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] std::optional<CollisionCellRef> SetCollisionCellBlocked(
    const CollisionCells& cells, std::int32_t tile_x, std::int32_t tile_z,
    std::int32_t layer);

// `cell_word` with its door passability set to blocked (5) when `blocked` is
// nonzero, or open (0).
[[nodiscard]] std::uint32_t BlockedCellWord(std::uint32_t cell_word,
                                            std::int32_t blocked);

// The cell under a world position, unless the cell's door passability closes
// the half (or the whole) of the tile the position is in.
[[nodiscard]] std::optional<CollisionCellRef> FindCollisionCellAtPosition(
    const CollisionCells& cells, std::int16_t world_x, std::int16_t world_z,
    std::int32_t layer);

// The packed floor height at a world position, smoothed toward the two
// nearest neighbouring samples within 64 units; kMissingFloorHeight off the
// grid or without a map.
[[nodiscard]] std::int16_t InterpolatedFloorHeight(
    std::span<const std::byte> collision_map, std::int32_t world_x,
    std::int32_t world_z);

// The packed floor height at a world position; kMissingFloorHeight off the
// grid or without a map.
[[nodiscard]] std::int16_t FloorHeightAt(
    std::span<const std::byte> collision_map, std::int32_t world_x,
    std::int32_t world_z);

// The packed ceiling height at a world position; a ceiling below the origin
// reads as -2032 (0xF810); 0 off the grid or without a map.
[[nodiscard]] std::int16_t CeilingHeightAt(
    std::span<const std::byte> collision_map, std::int32_t world_x,
    std::int32_t world_z);

struct FloorAndCeiling {
  std::int16_t floor = kMissingFloorHeight;
  std::int16_t ceiling = kMissingFloorHeight;

  friend bool operator==(const FloorAndCeiling&,
                         const FloorAndCeiling&) = default;
};

// Both packed heights; both kMissingFloorHeight off the grid or without a map.
[[nodiscard]] FloorAndCeiling FloorAndCeilingHeightsAt(
    std::span<const std::byte> collision_map, std::int32_t world_x,
    std::int32_t world_z);

// The ceiling height (bits 0..14 sign-extended), or 0 where none is packed.
[[nodiscard]] std::int32_t RoomCeilingHeight(
    std::span<const std::byte> collision_map, std::int32_t world_x,
    std::int32_t world_z);

// One room gimmick slot's state the support rules read.
struct GimmickSupportState {
  bool skipped = false;
  bool falling = false;
  std::uint8_t motion_state = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::int16_t mode = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};
inline constexpr std::size_t kGimmickSlotCount = 16;
inline constexpr std::int16_t kRemovedGimmickMode = 0xFE;
using GimmickSlots = std::span<const std::optional<GimmickSupportState>>;

// The interpolated floor height at a world position, raised to stand on any
// gimmick placed exactly there (128 units above its origin).
[[nodiscard]] std::int32_t ActorSupportHeight(
    std::span<const std::byte> collision_map, GimmickSlots gimmicks,
    std::int32_t world_x, std::int32_t world_z);

// An actor slot SnapRoomActorsToSupport may move.
struct RoomActorSupport {
  bool motion_locked = false;
  std::uint8_t tile_layer = 0;
  std::uint8_t movement_mode = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// Puts each free-standing actor on the room grid (not motion locked, layer 0
// or 1, no movement mode) at its support height. Returns how many moved.
std::size_t SnapRoomActorsToSupport(
    std::span<const std::byte> collision_map, GimmickSlots gimmicks,
    std::span<std::optional<RoomActorSupport>> actors);

struct GimmickSupport {
  std::int32_t height = 0;
  std::uint8_t gimmick_index = 0;

  friend bool operator==(const GimmickSupport&,
                         const GimmickSupport&) = default;
};

// The top of the highest resting gimmick on a tile (its origin less 128,
// never below 0) and its slot (0 when none).
[[nodiscard]] GimmickSupport GimmickSupportHeight(GimmickSlots gimmicks,
                                                  std::int32_t tile_x,
                                                  std::int32_t tile_z);

// The top of the highest standing gimmick on a tile below
// `reference_height` (larger is lower), never below 0.
[[nodiscard]] std::int32_t GimmickSupportHeightBelow(
    GimmickSlots gimmicks, std::int32_t tile_x, std::int32_t tile_z,
    std::int32_t reference_height);

// Whether a world position is within 64 units of the visible platform on
// `platform_layer`; false when no visible platform has that layer.
[[nodiscard]] bool IsPointOnMovingPlatform(
    std::span<const PlatformContact> platforms, std::int32_t platform_layer,
    std::int16_t world_x, std::int16_t world_z);

enum class PlatformSearch : std::int32_t {
  kStandOn = 0,
  kBelow = 1,
  kStepUp = 2,
  kDropBelow = 3,
};

struct PlatformContactHeight {
  std::int32_t height = 0;
  // The supporting platform's layer (2 + index), or 0.
  std::int32_t supporting_layer = 0;

  friend bool operator==(const PlatformContactHeight&,
                         const PlatformContactHeight&) = default;
};

// A world position probed against the moving platforms.
struct PlatformProbe {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

// The first visible platform within 64 units of a world position that the
// search mode accepts; modes outside 0..3 accept any. Height 0 when none.
[[nodiscard]] PlatformContactHeight MovingPlatformContactHeight(
    std::span<const PlatformContact> platforms, PlatformProbe probe,
    std::int32_t search_mode, std::int32_t search_depth);

// Bit 0: the actor passes through other actors. Bit 1: other actors pass
// through it.
struct CollisionBypass {
  bool actor_collision = false;
  bool ignored = false;

  friend bool operator==(const CollisionBypass&,
                         const CollisionBypass&) = default;
};

// The actor table's collision bypass bits.
class ActorCollisionFlags {
 public:
  ActorCollisionFlags() = default;
  ActorCollisionFlags(const ActorCollisionFlags&) = delete;
  ActorCollisionFlags& operator=(const ActorCollisionFlags&) = delete;
  ActorCollisionFlags(ActorCollisionFlags&&) = delete;
  ActorCollisionFlags& operator=(ActorCollisionFlags&&) = delete;
  virtual ~ActorCollisionFlags() = default;

  // nullopt for an empty actor slot.
  [[nodiscard]] virtual std::optional<CollisionBypass> Bypass(
      std::int32_t actor_index) const = 0;
  virtual void SetBypass(std::int32_t actor_index, CollisionBypass bypass) = 0;
};

// Sets an actor's bypass bits from `flags` (bits 0 and 1); an empty slot is
// left alone.
void SetActorCollisionBypass(ActorCollisionFlags& actors,
                             std::int32_t actor_index, std::int32_t flags);

// An actor's bypass bits as flags; 0 for an empty slot.
[[nodiscard]] std::int32_t ActorCollisionBypass(
    const ActorCollisionFlags& actors, std::int32_t actor_index);
}  // namespace VS::Field
