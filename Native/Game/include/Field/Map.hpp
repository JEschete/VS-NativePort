#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// Map progress rules: the minimap's visibility byte, the visit marks a loaded
// map scene shows, the tilted minimap rotation and the map completion share.
// Rooms are numbered by location id (0..511); location 0 is "no room".
namespace VS::Field {
inline constexpr std::size_t kRoomVisitFlagWords = 16;
inline constexpr std::int32_t kRoomLocationCount = 512;
inline constexpr std::int32_t kCountableRoomTotal = 361;

// One bit per location id (State_roomVisitProgress.roomFlags), or the mask of
// the locations completion counts.
using RoomVisitFlags = std::array<std::uint32_t, kRoomVisitFlagWords>;

// Stores the low byte of `visible` as the minimap's visibility byte and
// returns that byte (zero-extended, as the original's andi 0xFF).
std::uint8_t SetMinimapVisible(std::uint8_t& visibility, std::int32_t visible);

// The location id a scene room names (_getLocationId): 0 when no location
// matches.
class RoomLocationIndex {
 public:
  RoomLocationIndex() = default;
  RoomLocationIndex(const RoomLocationIndex&) = delete;
  RoomLocationIndex& operator=(const RoomLocationIndex&) = delete;
  RoomLocationIndex(RoomLocationIndex&&) = delete;
  RoomLocationIndex& operator=(RoomLocationIndex&&) = delete;
  virtual ~RoomLocationIndex() = default;

  [[nodiscard]] virtual std::int32_t LocationId(std::int32_t zone_id,
                                                std::int32_t map_index) = 0;
};

// Rewrites each room record's visited word in a map scene (MinimapScene.hpp)
// with the room's visit bit, in place (the bit itself, not 1). A location
// outside 1..511 leaves 0. Returns false and changes nothing when the scene
// has no room count or its room records do not fit in `scene`.
bool RestoreSceneRoomVisits(std::span<std::byte> scene,
                            const RoomVisitFlags& room_flags,
                            RoomLocationIndex& locations);

inline constexpr std::size_t kMinimapRotationOrder = 3;
using MinimapRotationRow = std::array<std::int16_t, kMinimapRotationOrder>;
using MinimapRotation = std::array<MinimapRotationRow, kMinimapRotationOrder>;

// The minimap's view rotation: a yaw about Y tilted by a fixed pitch (cosine
// 2633, sine 3138 in Q12), as the rows of a rotation matrix.
[[nodiscard]] MinimapRotation BuildMinimapRotation(std::int32_t yaw_angle);

// The share of countable locations visited, 0..100 (count * 100 / 361).
// Having visited location 55 marks location 54 first, as the original does.
[[nodiscard]] std::int32_t MapCompletionPercent(
    RoomVisitFlags& room_flags, const RoomVisitFlags& countable_rooms);
}  // namespace VS::Field
