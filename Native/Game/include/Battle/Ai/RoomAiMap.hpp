#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include "Battle/Ai/TileHeightSampling.hpp"
#include "Core/Math/Distance.hpp"

// The room AI map: per-tile movement flags and navigation heights built from a
// room's collision data, plus the room's AI settings. Building is a pure
// function of an explicit room source; the battle runtime publishes the result.
namespace VS::Battle::Ai {
inline constexpr int kRoomGridSide = 32;
// Bytes of the room's auxiliary AI section that the map header reads.
inline constexpr std::size_t kRoomAuxiliaryHeaderBytes = 0x498;
inline constexpr std::uint8_t kDefaultActionDelay = 150;
inline constexpr std::uint16_t kBlockedTileFlag = 0x8000;

struct RoomCollisionExtent {
  int width = 0;
  int height = 0;
};

// The floor trigger on a tile, as stored in the room (both fields unchecked).
struct RoomFloorTrigger {
  std::uint16_t kind = 0;
  std::uint16_t switch_value = 0;
};

// Everything the room AI map reads about the current room.
struct RoomAiSource {
  // Collision grid size, absent when the room has no collision map.
  std::optional<RoomCollisionExtent> collision_extent;
  // Raw 32-bit collision cell of a tile; absent outside the collision grid.
  std::function<std::optional<std::uint32_t>(Core::Math::TileCoordinate)>
      collision_cell;
  FloorHeightQuery floor_height;
  std::function<std::optional<RoomFloorTrigger>(Core::Math::TileCoordinate)>
      floor_trigger;
  // Room environment height parameter, absent when the room has none.
  std::optional<int> environment_parameter;
  // The room's auxiliary AI section, absent when the room has none.
  std::optional<std::span<const std::byte>> auxiliary;
};

struct RoomAiSettings {
  std::uint8_t max_tile_x = 0;
  std::uint8_t max_tile_z = 0;
  std::uint8_t min_tile_x = 0;
  std::uint8_t min_tile_z = 0;
  // The collision grid size, kept beside the bounds as the original does.
  std::array<std::uint8_t, 2> collision_extent{};
  std::uint8_t default_action_delay = kDefaultActionDelay;
  bool allows_non_spell_actions = false;
  std::uint8_t engagement_limit = 0;
  int flight_ceiling = 0;
};

using RoomAiRow = std::array<std::uint16_t, kRoomGridSide>;

struct RoomAiGrid {
  RoomAiSettings settings;
  std::array<RoomAiRow, kRoomGridSide> flags{};
  std::array<RoomAiRow, kRoomGridSide> heights{};
};

// Movement flags from a raw collision cell word.
[[nodiscard]] std::uint16_t DecodeCollisionFlags(std::uint32_t cell) noexcept;

// Builds the map, or nothing when the room header is invalid: a collision grid
// larger than 32x32 or an auxiliary section shorter than its header.
[[nodiscard]] std::optional<RoomAiGrid> BuildRoomAiMap(
    const RoomAiSource& source);

// What the room AI lifecycle needs from the battle runtime that owns the
// published map and the actors' AI states.
class RoomAiRuntime {
 public:
  virtual ~RoomAiRuntime() = default;
  [[nodiscard]] virtual RoomAiSource CurrentRoom() const = 0;
  // Stores and publishes the map; aborts if storage cannot be allocated.
  virtual void PublishRoomAiMap(const RoomAiGrid& grid) = 0;
  // Settings of the published map, absent while none is published.
  [[nodiscard]] virtual std::optional<RoomAiSettings> PublishedSettings()
      const = 0;
  // Rebuilds state derived from the published map: actor working state,
  // terrain heights and route gate availability.
  virtual void RefreshDerivedRoomState() = 0;
  virtual void FreeActorAiStates() = 0;
  // Frees the published map and clears every binding to it.
  virtual void ReleaseRoomAiMap() = 0;
};

// The battle runtime's room AI storage.
[[nodiscard]] RoomAiRuntime& BattleRoomAiRuntime();

// Builds and publishes the current room's AI map before actors are
// initialized (func_800E6F9C). Aborts on an invalid room header.
void InitializeRoomAiMap(RoomAiRuntime& runtime);

// Initializes the room's AI navigation on room entry: the original only
// built and published the room AI map (func_800E6158).
void InitializeRoomNavigation(RoomAiRuntime& runtime);

// Frees every actor's AI state and the room AI map (func_800E6F1C).
void FreeRoomAiState(RoomAiRuntime& runtime);
}  // namespace VS::Battle::Ai
