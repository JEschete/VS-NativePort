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

std::uint32_t ReadWord(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < sizeof value; ++index) {
    value |= static_cast<std::uint32_t>(bytes[offset + index]) << (8 * index);
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
      source.floorTrigger ? source.floorTrigger(tile) : std::nullopt;
  if (trigger && trigger->kind < kTriggerKindCount &&
      trigger->switchValue < kTriggerSwitchCount) {
    flags |= kTriggerFlag << trigger->switchValue;
  }
  return static_cast<std::uint16_t>(flags);
}
}  // namespace

std::uint16_t DecodeCollisionFlags(std::uint32_t cell) noexcept {
  struct Field {
    unsigned shift;
    unsigned outputMask;
  };
  static constexpr std::array<Field, 6> kFields{{{23, 0x1F},
                                                 {12, 0x20},
                                                 {22, 0x40},
                                                 {20, 0x800},
                                                 {18, 0x1000},
                                                 {16, 0x2000}}};
  unsigned flags = 0;
  for (const Field& field : kFields) {
    flags |= (cell >> field.shift) & field.outputMask;
  }
  return static_cast<std::uint16_t>(flags);
}

// decomp: BATTLE.PRG func_800E6F9C (src/BATTLE/BATTLE.PRG/7E05C.s:582), the
// map-building half.
std::optional<RoomAiGrid> BuildRoomAiMap(const RoomAiSource& source) {
  if ((source.auxiliary &&
       source.auxiliary->size() < kRoomAuxiliaryHeaderBytes) ||
      !ExtentFits(source.collisionExtent)) {
    Core_Errorf("ai", "Invalid room AI header: auxiliary_bytes=%zu",
                source.auxiliary ? source.auxiliary->size() : std::size_t{0});
    return std::nullopt;
  }
  std::optional<RoomAiGrid> result{std::in_place};
  RoomAiGrid& grid = *result;
  RoomAiSettings& settings = grid.settings;
  if (source.collisionExtent) {
    settings.maxTileX =
        static_cast<std::uint8_t>(source.collisionExtent->width);
    settings.maxTileZ =
        static_cast<std::uint8_t>(source.collisionExtent->height);
  }
  settings.collisionExtent[0] = settings.maxTileX;
  settings.collisionExtent[1] = settings.maxTileZ;
  if (source.environmentParameter) {
    settings.flightCeiling =
        -*source.environmentParameter * (1 << kHeightFractionBits);
  }
  for (int tile_z = 0; tile_z < kRoomGridSide; ++tile_z) {
    for (int tile_x = 0; tile_x < kRoomGridSide; ++tile_x) {
      const Core::Math::TileCoordinate tile{static_cast<std::uint8_t>(tile_x),
                                            static_cast<std::uint8_t>(tile_z)};
      std::uint16_t& flags = grid.flags[static_cast<std::size_t>(tile_z)]
                                       [static_cast<std::size_t>(tile_x)];
      const std::optional<std::uint32_t> cell =
          source.collisionCell ? source.collisionCell(tile) : std::nullopt;
      if (!cell) {
        flags = kBlockedTileFlag;
        continue;
      }
      flags = TileFlags(source, tile, *cell);
      grid.heights[static_cast<std::size_t>(tile_z)]
                  [static_cast<std::size_t>(tile_x)] =
          static_cast<std::uint16_t>(
              (SampleAiTileGroundHeight(source.floorHeight, tile) >>
               kHeightFractionBits) &
              kStoredHeightMask);
    }
  }
  if (source.auxiliary) {
    const std::span<const std::byte> auxiliary = *source.auxiliary;
    const std::uint32_t room_settings =
        ReadWord(auxiliary, kRoomSettingsOffset);
    const std::uint32_t bounds = ReadWord(auxiliary, kRoomBoundsOffset);
    settings.maxTileX = static_cast<std::uint8_t>(bounds);
    settings.maxTileZ = static_cast<std::uint8_t>(bounds >> 8);
    settings.minTileX = static_cast<std::uint8_t>(bounds >> 16);
    settings.minTileZ = static_cast<std::uint8_t>(bounds >> 24);
    settings.defaultActionDelay =
        static_cast<std::uint8_t>(room_settings >> kActionDelayShift);
    settings.allowsNonSpellActions =
        ((room_settings >> kNonSpellActionsShift) & 1) != 0;
    settings.engagementLimit =
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
