// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/RoomAiMap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Battle/Ai/TileHeightSampling.hpp"
#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr std::size_t kRoomSettingsOffset = 0x450;
constexpr std::size_t kRoomBoundsOffset = 0x494;
constexpr int kHeightFractionBits = 6;
constexpr int kStoredHeightMask = 0x3FF;
constexpr std::uint16_t kTriggerKindCount = 16;
constexpr std::uint16_t kTriggerSwitchCount = 2;
constexpr unsigned kTriggerFlag = 0x100;
constexpr unsigned kActionDelayShift = 21;
constexpr unsigned kNonSpellActionsShift = 20;
constexpr unsigned kEngagementLimitShift = 29;
constexpr unsigned kBitsPerByte = 8;
constexpr unsigned kMaxTileZShift = 8;
constexpr unsigned kMinTileXShift = 16;
constexpr unsigned kMinTileZShift = 24;

std::uint32_t ReadWord(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < sizeof value; ++index) {
    value |= static_cast<std::uint32_t>(bytes[offset + index])
             << (kBitsPerByte * index);
  }
  return value;
}

bool ExtentFits(const std::optional<RoomCollisionExtent>& extent) {
  return !extent || (extent->width >= 0 && extent->width <= kRoomGridSide &&
                     extent->height >= 0 && extent->height <= kRoomGridSide);
}

std::uint16_t TileFlags(const RoomAiSource& source,
                        Core::Math::TileCoordinate tile, std::uint32_t cell) {
  unsigned flags = DecodeCollisionFlags(cell);
  const std::optional<RoomFloorTrigger> trigger =
      source.floor_trigger ? source.floor_trigger(tile) : std::nullopt;
  if (trigger && trigger->kind < kTriggerKindCount &&
      trigger->switch_value < kTriggerSwitchCount) {
    flags |= kTriggerFlag << trigger->switch_value;
  }
  return static_cast<std::uint16_t>(flags);
}
}  // namespace

std::uint16_t DecodeCollisionFlags(std::uint32_t cell) noexcept {
  struct Field {
    unsigned shift;
    unsigned output_mask;
  };
  static constexpr std::array<Field, 6> kFields{{{23, 0x1F},
                                                 {12, 0x20},
                                                 {22, 0x40},
                                                 {20, 0x800},
                                                 {18, 0x1000},
                                                 {16, 0x2000}}};
  unsigned flags = 0;
  for (const Field& field : kFields) {
    flags |= (cell >> field.shift) & field.output_mask;
  }
  return static_cast<std::uint16_t>(flags);
}

// decomp: BATTLE.PRG func_800E6F9C (src/BATTLE/BATTLE.PRG/7E05C.s:582), the
// map-building half.
std::optional<RoomAiGrid> BuildRoomAiMap(const RoomAiSource& source) {
  if ((source.auxiliary &&
       source.auxiliary->size() < kRoomAuxiliaryHeaderBytes) ||
      !ExtentFits(source.collision_extent)) {
    Core_Errorf("ai", "Invalid room AI header: auxiliary_bytes=%zu",
                source.auxiliary ? source.auxiliary->size() : std::size_t{0});
    return std::nullopt;
  }
  std::optional<RoomAiGrid> result{std::in_place};
  RoomAiGrid& grid = *result;
  RoomAiSettings& settings = grid.settings;
  if (source.collision_extent) {
    settings.max_tile_x =
        static_cast<std::uint8_t>(source.collision_extent->width);
    settings.max_tile_z =
        static_cast<std::uint8_t>(source.collision_extent->height);
  }
  settings.collision_extent[0] = settings.max_tile_x;
  settings.collision_extent[1] = settings.max_tile_z;
  if (source.environment_parameter) {
    settings.flight_ceiling =
        -*source.environment_parameter * (1 << kHeightFractionBits);
  }
  for (int tile_z = 0; tile_z < kRoomGridSide; ++tile_z) {
    for (int tile_x = 0; tile_x < kRoomGridSide; ++tile_x) {
      const Core::Math::TileCoordinate tile{static_cast<std::uint8_t>(tile_x),
                                            static_cast<std::uint8_t>(tile_z)};
      std::uint16_t& flags = grid.flags[static_cast<std::size_t>(tile_z)]
                                       [static_cast<std::size_t>(tile_x)];
      const std::optional<std::uint32_t> cell =
          source.collision_cell ? source.collision_cell(tile) : std::nullopt;
      if (!cell) {
        flags = kBlockedTileFlag;
        continue;
      }
      flags = TileFlags(source, tile, *cell);
      grid.heights[static_cast<std::size_t>(tile_z)]
                  [static_cast<std::size_t>(tile_x)] =
          static_cast<std::uint16_t>(
              (SampleAiTileGroundHeight(source.floor_height, tile) >>
               kHeightFractionBits) &
              kStoredHeightMask);
    }
  }
  if (source.auxiliary) {
    const std::span<const std::byte> auxiliary = *source.auxiliary;
    const std::uint32_t room_settings =
        ReadWord(auxiliary, kRoomSettingsOffset);
    const std::uint32_t bounds = ReadWord(auxiliary, kRoomBoundsOffset);
    // The bounds word packs max x, max z, min x and min z, one per byte.
    settings.max_tile_x = static_cast<std::uint8_t>(bounds);
    settings.max_tile_z = static_cast<std::uint8_t>(bounds >> kMaxTileZShift);
    settings.min_tile_x = static_cast<std::uint8_t>(bounds >> kMinTileXShift);
    settings.min_tile_z = static_cast<std::uint8_t>(bounds >> kMinTileZShift);
    settings.default_action_delay =
        static_cast<std::uint8_t>(room_settings >> kActionDelayShift);
    settings.allows_non_spell_actions =
        ((room_settings >> kNonSpellActionsShift) & 1) != 0;
    settings.engagement_limit =
        static_cast<std::uint8_t>(room_settings >> kEngagementLimitShift);
  }
  return result;
}

// decomp: BATTLE.PRG func_800E6F9C (src/BATTLE/BATTLE.PRG/7E05C.s:582).
void InitializeRoomAiMap(RoomAiRuntime& runtime) {
  const std::optional<RoomAiGrid> grid = BuildRoomAiMap(runtime.CurrentRoom());
  if (!grid) {
    Core_Errorf("ai", "Room AI initialization rejected the room header");
    std::abort();
  }
  runtime.PublishRoomAiMap(*grid);
  // Actor creation can query terrain before the first AI update.
  runtime.RefreshDerivedRoomState();
}

// decomp: BATTLE.PRG func_800E6F1C (src/BATTLE/BATTLE.PRG/7E05C.s:541).
void FreeRoomAiState(RoomAiRuntime& runtime) {
  runtime.FreeActorAiStates();
  runtime.ReleaseRoomAiMap();
}
}  // namespace VS::Battle::Ai
