// Hand-maintained (tools/port/owned.csv).
#include "Field/CollisionMap.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>

#include "Field/CollisionSampling.hpp"
#include "Field/PlatformPlacement.hpp"

namespace VS::Field {
namespace {
constexpr std::uint32_t kPassabilityShift = 10;
constexpr std::uint32_t kPassabilityMask = 7;
constexpr int kSampleShift = 5;
constexpr int kTileShift = 7;
constexpr int kSamplesPerTileShift = kTileShift - kSampleShift;
constexpr std::int32_t kSampleOffsetMask = (1 << kSampleShift) - 1;
constexpr std::int32_t kSampleCenter = 1 << (kSampleShift - 1);
constexpr std::int32_t kHeightCodeStep = 2;
constexpr std::int32_t kHeightUnit = 8;
constexpr std::uint8_t kHeightCodeValue = 0x7F;
constexpr std::uint8_t kHeightCodeInvalid = 0x80;
constexpr int kInvalidBitShift = 8;
constexpr std::int32_t kPackedHeightMask = 0x7FFF;
constexpr std::int32_t kPackedSignBit = 0x4000;
constexpr std::int32_t kPackedRange = 0x8000;
constexpr std::int16_t kCeilingBelowOrigin = -2032;  // 0xF810
constexpr std::int32_t kNeighborSmoothingLimit = 0x40;
constexpr std::int32_t kWeightTotal = 0x400;
constexpr std::int32_t kWeightPerOffset = 32;
constexpr int kDimensionHeightShift = 20;
constexpr std::int32_t kDimensionWidthScale = 16;
constexpr std::uint32_t kDimensionWidthMask = 0xFFFF;
constexpr std::int32_t kGimmickTopOffset = 128;
constexpr std::int32_t kNoGimmickSupport = 3000;
constexpr std::int32_t kPlatformReach = 64;
constexpr std::int32_t kPlatformStandReach = 256;
constexpr std::int32_t kPlatformStepReach = 128;
constexpr std::int32_t kPlatformTopOffset = 96;
constexpr std::uint8_t kGroundLayers = 2;

enum class Passability : std::uint32_t {
  kOpen = 0,
  kClosedAboveSum = 1,
  kClosedAboveDifference = 2,
  kClosedBelowSum = 3,
  kClosedBelowDifference = 4,
};

// The original compares sign-extended halfwords against unsigned tile indices.
std::uint32_t GridExtent(std::int16_t extent) {
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(extent));
}

std::int32_t DecodedHeight(std::int16_t origin, std::uint8_t code) {
  return (origin - (code & kHeightCodeValue) * kHeightCodeStep) * kHeightUnit;
}

std::int32_t InvalidBit(std::uint8_t code) {
  return (code & kHeightCodeInvalid) << kInvalidBitShift;
}

std::int16_t PackHeight(std::int32_t height, std::uint8_t code) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(
      (height & kPackedHeightMask) | InvalidBit(code)));
}

// Bits 0..14 of a packed height, sign-extended (the original's << 17 >> 17).
std::int32_t UnpackHeight(std::int16_t packed) {
  const std::int32_t value = packed & kPackedHeightMask;
  return (value & kPackedSignBit) != 0 ? value - kPackedRange : value;
}

std::uint8_t SampleCode(std::span<const std::byte> map, std::int32_t sample_x,
                        std::int32_t sample_z, int layer) {
  return SampleCollisionHeightCode(map, static_cast<std::uint32_t>(sample_x),
                                   sample_z, layer);
}

bool TileInGrid(const CollisionMapHeader& map, std::int32_t world_x,
                std::int32_t world_z) {
  return static_cast<std::uint32_t>(world_x >> kTileShift) <
             GridExtent(map.width) &&
         static_cast<std::uint32_t>(world_z >> kTileShift) <
             GridExtent(map.height);
}

std::optional<CollisionCellRef> PlatformCell(const CollisionCells& cells,
                                             std::int32_t layer) {
  const auto index = static_cast<std::size_t>(layer - kPlatformCollisionLayer);
  if (index >= cells.platforms.size()) return std::nullopt;
  return CollisionCellRef{.source = CollisionCellSource::kPlatform,
                          .index = index};
}

std::uint32_t CellWord(const CollisionCells& cells, CollisionCellRef cell) {
  return cell.source == CollisionCellSource::kRoom
             ? cells.room[cell.index]
             : cells.platforms[cell.index];
}

// A neighbouring sample's height, or `base` when the neighbour is off the
// grid or more than a step away.
std::int32_t SmoothedNeighbor(std::span<const std::byte> map,
                              const CollisionMapHeader& header,
                              std::int32_t base, bool in_grid,
                              std::int32_t sample_x, std::int32_t sample_z) {
  if (!in_grid) return base;
  const std::int32_t height = DecodedHeight(
      header.height_origin, SampleCode(map, sample_x, sample_z, 0));
  return std::abs(height - base) >= kNeighborSmoothingLimit ? base : height;
}

bool InPlatformReach(const PlatformContact& platform, std::int32_t world_x,
                     std::int32_t world_z) {
  return world_x < platform.x + kPlatformReach &&
         world_x >= platform.x - kPlatformReach &&
         world_z < platform.z + kPlatformReach &&
         world_z >= platform.z - kPlatformReach;
}

std::int32_t GimmickTop(std::int32_t lowest_origin) {
  return std::min(lowest_origin - kGimmickTopOffset, 0);
}
}  // namespace

std::optional<CollisionMapHeader> ReadCollisionMapHeader(
    std::span<const std::byte> collision_map) {
  if (collision_map.size() < sizeof(CollisionMapHeader)) return std::nullopt;
  CollisionMapHeader header;
  std::memcpy(&header, collision_map.data(), sizeof header);
  return header;
}

std::uint32_t CellPassability(std::uint32_t cell_word) {
  return (cell_word >> kPassabilityShift) & kPassabilityMask;
}

// decomp: BATTLE.PRG func_8008B6EC (src/BATTLE/BATTLE.PRG/146C.c:16059)
std::span<const std::uint32_t> RoomCollisionCells(const CollisionCells& cells) {
  const std::span<const std::uint32_t> room = cells.room;
  return room;
}

// decomp: BATTLE.PRG _getCollisionMapDimensions
// (src/BATTLE/BATTLE.PRG/146C.c:16063)
std::int32_t CollisionMapDimensions(std::optional<CollisionMapHeader> map) {
  if (!map) return 0;
  const auto rows = static_cast<std::uint32_t>(map->height)
                    << kDimensionHeightShift;
  const auto columns =
      static_cast<std::uint32_t>(map->width * kDimensionWidthScale) &
      kDimensionWidthMask;
  return static_cast<std::int32_t>(rows | columns);
}

// decomp: BATTLE.PRG _getMapBounds (src/BATTLE/BATTLE.PRG/146C.c:16072). The
// original returns the section pointer; a section shorter than one record is
// treated as missing.
std::optional<CameraBounds> RoomCameraBounds(
    std::span<const std::byte> zone_bounds) {
  if (zone_bounds.size() < sizeof(CameraBounds)) return std::nullopt;
  CameraBounds bounds;
  std::memcpy(&bounds, zone_bounds.data(), sizeof bounds);
  return bounds;
}

// decomp: BATTLE.PRG func_8008B764 (src/BATTLE/BATTLE.PRG/146C.c:16080). The
// original reads past the cell and platform tables and through a missing
// collision map; those lookups find no cell.
std::optional<CollisionCellRef> CollisionCellAt(const CollisionCells& cells,
                                                std::uint32_t tile_x,
                                                std::uint32_t tile_z,
                                                std::int32_t layer) {
  if (cells.room.empty()) return std::nullopt;
  if (layer >= kPlatformCollisionLayer) return PlatformCell(cells, layer);
  if (!cells.map) return std::nullopt;
  const std::uint32_t width = GridExtent(cells.map->width);
  if (tile_x >= width || tile_z >= GridExtent(cells.map->height)) {
    return std::nullopt;
  }
  const std::uint32_t index = (tile_z * width) + tile_x;
  if (index >= cells.room.size()) return std::nullopt;
  return CollisionCellRef{.source = CollisionCellSource::kRoom, .index = index};
}

// decomp: BATTLE.PRG func_8008B808 (src/BATTLE/BATTLE.PRG/146C.c:16099). The
// original writes any index the arguments form; cells outside the tables are
// refused.
std::optional<CollisionCellRef> SetCollisionCellBlocked(
    const CollisionCells& cells, std::int32_t tile_x, std::int32_t tile_z,
    std::int32_t layer) {
  if (cells.room.empty()) return std::nullopt;
  if (layer >= kPlatformCollisionLayer) return PlatformCell(cells, layer);
  if (!cells.map) return std::nullopt;
  const std::int64_t width = cells.map->width;
  const std::int64_t index =
      (layer * width * cells.map->height) + (tile_z * width) + tile_x;
  if (index < 0 || static_cast<std::uint64_t>(index) >= cells.room.size()) {
    return std::nullopt;
  }
  return CollisionCellRef{.source = CollisionCellSource::kRoom,
                          .index = static_cast<std::size_t>(index)};
}

std::uint32_t BlockedCellWord(std::uint32_t cell_word, std::int32_t blocked) {
  const std::uint32_t passability = blocked != 0 ? kBlockedPassability : 0;
  return (cell_word & ~(kPassabilityMask << kPassabilityShift)) |
         (passability << kPassabilityShift);
}

// decomp: BATTLE.PRG func_800A8D64 (src/BATTLE/BATTLE.PRG/40564.c:139). The
// unused passabilities 6 and 7 find no cell, as the jump table's bound does.
std::optional<CollisionCellRef> FindCollisionCellAtPosition(
    const CollisionCells& cells, std::int16_t world_x, std::int16_t world_z,
    std::int32_t layer) {
  const std::int32_t tile_x = world_x / kTileWorldSize;
  const std::int32_t tile_z = world_z / kTileWorldSize;
  const auto cell = CollisionCellAt(cells, static_cast<std::uint32_t>(tile_x),
                                    static_cast<std::uint32_t>(tile_z), layer);
  if (!cell) return std::nullopt;
  const std::int32_t local_x = world_x - (tile_x * kTileWorldSize);
  const std::int32_t local_z = world_z - (tile_z * kTileWorldSize);
  bool closed = false;
  switch (static_cast<Passability>(CellPassability(CellWord(cells, *cell)))) {
    case Passability::kOpen:
      break;
    case Passability::kClosedAboveSum:
      closed = local_x + local_z > kTileWorldSize;
      break;
    case Passability::kClosedAboveDifference:
      closed = local_x - local_z > 0;
      break;
    case Passability::kClosedBelowSum:
      closed = local_x + local_z < kTileWorldSize;
      break;
    case Passability::kClosedBelowDifference:
      closed = local_x - local_z < 0;
      break;
    default:
      closed = true;
      break;
  }
  if (closed) return std::nullopt;
  return cell;
}

// decomp: BATTLE.PRG func_8008DA24 (src/BATTLE/BATTLE.PRG/146C.c:17000). The
// weights are the GTE NCLIP products 32 * |offset - 16| per axis.
std::int16_t InterpolatedFloorHeight(std::span<const std::byte> collision_map,
                                     std::int32_t world_x,
                                     std::int32_t world_z) {
  const auto header = ReadCollisionMapHeader(collision_map);
  if (!header) return kMissingFloorHeight;
  const std::int32_t offset_x = world_x & kSampleOffsetMask;
  const std::int32_t offset_z = world_z & kSampleOffsetMask;
  const std::int32_t sample_x = world_x >> kSampleShift;
  const std::int32_t sample_z = world_z >> kSampleShift;
  const std::uint32_t width = GridExtent(header->width);
  const std::uint32_t height = GridExtent(header->height);
  if (static_cast<std::uint32_t>(sample_x >> kSamplesPerTileShift) >= width ||
      static_cast<std::uint32_t>(sample_z >> kSamplesPerTileShift) >= height) {
    return kMissingFloorHeight;
  }
  const std::uint8_t code = SampleCode(collision_map, sample_x, sample_z, 0);
  std::int32_t base = DecodedHeight(header->height_origin, code);
  const std::int32_t neighbor_x =
      offset_x < kSampleCenter ? sample_x - 1 : sample_x + 1;
  const std::int32_t neighbor_z =
      offset_z >= kSampleCenter ? sample_z + 1 : sample_z - 1;
  const std::int32_t height_x = SmoothedNeighbor(
      collision_map, *header, base,
      static_cast<std::uint32_t>(neighbor_x >> kSamplesPerTileShift) < width,
      neighbor_x, sample_z);
  const std::int32_t height_z = SmoothedNeighbor(
      collision_map, *header, base,
      static_cast<std::uint32_t>(neighbor_z >> kSamplesPerTileShift) < height,
      sample_x, neighbor_z);
  const std::int32_t weight_x =
      kWeightPerOffset * std::abs(offset_x - kSampleCenter);
  const std::int32_t weight_z =
      kWeightPerOffset * std::abs(offset_z - kSampleCenter);
  const std::int32_t weight_center = kWeightTotal - weight_x - weight_z;
  if (weight_center >= 0) {
    base = ((height_z * weight_z) + (height_x * weight_x) +
            (base * weight_center)) /
           kWeightTotal;
  }
  return PackHeight(base, code);
}

// decomp: BATTLE.PRG func_8008DC7C (src/BATTLE/BATTLE.PRG/146C.c:17083)
std::int16_t FloorHeightAt(std::span<const std::byte> collision_map,
                           std::int32_t world_x, std::int32_t world_z) {
  const auto header = ReadCollisionMapHeader(collision_map);
  if (!header || !TileInGrid(*header, world_x, world_z)) {
    return kMissingFloorHeight;
  }
  const std::uint8_t code = SampleCode(collision_map, world_x >> kSampleShift,
                                       world_z >> kSampleShift, 0);
  return PackHeight(DecodedHeight(header->height_origin, code), code);
}

// decomp: BATTLE.PRG func_8008DD0C (src/BATTLE/BATTLE.PRG/146C.c:17097)
std::int16_t CeilingHeightAt(std::span<const std::byte> collision_map,
                             std::int32_t world_x, std::int32_t world_z) {
  const auto header = ReadCollisionMapHeader(collision_map);
  if (!header || !TileInGrid(*header, world_x, world_z)) return 0;
  const std::uint8_t code = SampleCode(collision_map, world_x >> kSampleShift,
                                       world_z >> kSampleShift, 1);
  const std::int32_t ceiling = DecodedHeight(header->height_origin, code);
  return ceiling > 0 ? kCeilingBelowOrigin : PackHeight(ceiling, code);
}

// decomp: BATTLE.PRG func_8008DDA8 (src/BATTLE/BATTLE.PRG/146C.c:17118)
FloorAndCeiling FloorAndCeilingHeightsAt(
    std::span<const std::byte> collision_map, std::int32_t world_x,
    std::int32_t world_z) {
  const auto header = ReadCollisionMapHeader(collision_map);
  if (!header || !TileInGrid(*header, world_x, world_z)) return {};
  FloorAndCeiling heights{
      .floor = FloorHeightAt(collision_map, world_x, world_z),
      .ceiling = CeilingHeightAt(collision_map, world_x, world_z)};
  return heights;
}

// decomp: BATTLE.PRG func_800A3BC4 (src/BATTLE/BATTLE.PRG/3A1A0.c:652)
std::int32_t RoomCeilingHeight(std::span<const std::byte> collision_map,
                               std::int32_t world_x, std::int32_t world_z) {
  const std::int16_t packed = CeilingHeightAt(collision_map, world_x, world_z);
  return packed == 0 ? 0 : UnpackHeight(packed);
}

// decomp: BATTLE.PRG func_800A3500 (src/BATTLE/BATTLE.PRG/3A1A0.c:394)
std::int32_t ActorSupportHeight(std::span<const std::byte> collision_map,
                                GimmickSlots gimmicks, std::int32_t world_x,
                                std::int32_t world_z) {
  std::int32_t height =
      UnpackHeight(InterpolatedFloorHeight(collision_map, world_x, world_z));
  for (const auto& gimmick : gimmicks) {
    if (gimmick && gimmick->x == world_x && gimmick->z == world_z &&
        gimmick->y - kGimmickTopOffset < height) {
      height = gimmick->y - kGimmickTopOffset;
    }
  }
  return height;
}

// decomp: BATTLE.PRG func_800A35A8 (src/BATTLE/BATTLE.PRG/3A1A0.c:414)
std::size_t SnapRoomActorsToSupport(
    std::span<const std::byte> collision_map, GimmickSlots gimmicks,
    std::span<std::optional<RoomActorSupport>> actors) {
  std::size_t moved = 0;
  for (auto& actor : actors) {
    if (!actor || actor->motion_locked || actor->tile_layer >= kGroundLayers ||
        actor->movement_mode != 0) {
      continue;
    }
    actor->y = static_cast<std::int16_t>(
        ActorSupportHeight(collision_map, gimmicks, actor->x, actor->z));
    ++moved;
  }
  return moved;
}

// decomp: BATTLE.PRG func_800A92B8 (src/BATTLE/BATTLE.PRG/40564.c:343)
GimmickSupport GimmickSupportHeight(GimmickSlots gimmicks, std::int32_t tile_x,
                                    std::int32_t tile_z) {
  std::int32_t lowest = kNoGimmickSupport;
  std::uint8_t index = 0;
  for (std::size_t slot = 0; slot < gimmicks.size(); ++slot) {
    const auto& gimmick = gimmicks[slot];
    if (gimmick && !gimmick->skipped && gimmick->tile_x == tile_x &&
        gimmick->tile_z == tile_z && gimmick->motion_state == 0 &&
        gimmick->mode != kRemovedGimmickMode && gimmick->y < lowest) {
      lowest = gimmick->y;
      index = static_cast<std::uint8_t>(slot);
    }
  }
  return GimmickSupport{.height = GimmickTop(lowest), .gimmick_index = index};
}

// decomp: BATTLE.PRG func_800B13CC (src/BATTLE/BATTLE.PRG/44F14.c:2020)
std::int32_t GimmickSupportHeightBelow(GimmickSlots gimmicks,
                                       std::int32_t tile_x, std::int32_t tile_z,
                                       std::int32_t reference_height) {
  std::int32_t lowest = kNoGimmickSupport;
  for (const auto& gimmick : gimmicks) {
    if (gimmick && !gimmick->skipped && !gimmick->falling &&
        gimmick->tile_x == tile_x && gimmick->tile_z == tile_z &&
        reference_height < gimmick->y && gimmick->y < lowest) {
      lowest = gimmick->y;
    }
  }
  return GimmickTop(lowest);
}

// decomp: BATTLE.PRG func_800A51A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:1307). The
// original dereferences a null contact when no visible platform has the
// layer; that position is not on a platform.
bool IsPointOnMovingPlatform(std::span<const PlatformContact> platforms,
                             std::int32_t platform_layer, std::int16_t world_x,
                             std::int16_t world_z) {
  const std::int32_t index = platform_layer - kPlatformCollisionLayer;
  const auto platform =
      std::ranges::find(platforms, index, &PlatformContact::platform_index);
  if (platform == platforms.end()) return false;
  return world_x <= platform->x + kPlatformReach &&
         world_x >= platform->x - kPlatformReach &&
         world_z <= platform->z + kPlatformReach &&
         world_z >= platform->z - kPlatformReach;
}

// decomp: BATTLE.PRG func_800A9378 (src/BATTLE/BATTLE.PRG/40564.c:377). Modes
// 1 and 3 report the platform's top (+96) without making it the support.
PlatformContactHeight MovingPlatformContactHeight(
    std::span<const PlatformContact> platforms, PlatformProbe probe,
    std::int32_t search_mode, std::int32_t search_depth) {
  const std::int32_t world_y = probe.y;
  for (const auto& platform : platforms) {
    if (!InPlatformReach(platform, probe.x, probe.z)) continue;
    const std::int32_t top = platform.height;
    switch (static_cast<PlatformSearch>(search_mode)) {
      case PlatformSearch::kStandOn:
        if (top < world_y - kPlatformStandReach) continue;
        break;
      case PlatformSearch::kBelow:
        if (top < world_y - kPlatformStandReach) {
          return {.height = top + kPlatformTopOffset};
        }
        continue;
      case PlatformSearch::kStepUp:
        if (top >= world_y || top < world_y - kPlatformStepReach) continue;
        break;
      case PlatformSearch::kDropBelow:
        if (top >= world_y || top < world_y - search_depth) continue;
        return {.height = top + kPlatformTopOffset};
      default:
        break;
    }
    return {
        .height = top,
        .supporting_layer = platform.platform_index + kPlatformCollisionLayer};
  }
  return {};
}

// decomp: BATTLE.PRG func_800A0A1C (src/BATTLE/BATTLE.PRG/30DB0.c:3994)
void SetActorCollisionBypass(ActorCollisionFlags& actors,
                             std::int32_t actor_index, std::int32_t flags) {
  if (!actors.Bypass(actor_index)) return;
  actors.SetBypass(actor_index,
                   CollisionBypass{.actor_collision = (flags & 1) != 0,
                                   .ignored = (flags & 2) != 0});
}

// decomp: BATTLE.PRG func_800A0A74 (src/BATTLE/BATTLE.PRG/30DB0.c:4003). An
// empty slot returns the null record pointer, 0.
std::int32_t ActorCollisionBypass(const ActorCollisionFlags& actors,
                                  std::int32_t actor_index) {
  const auto bypass = actors.Bypass(actor_index);
  if (!bypass) return 0;
  return (bypass->actor_collision ? 1 : 0) | (bypass->ignored ? 2 : 0);
}
}  // namespace VS::Field
