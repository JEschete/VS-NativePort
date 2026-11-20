#pragma once
// A room enemy's remembered state, as the recent-room cache stores it
// (VS::State::recentRoomEnemyStates) and the room spawner reads it back.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace VS::Actors {
inline constexpr std::size_t kRoomActorRecordLimbs = 6;

// decomp: vs_SavedEnemyState (src/SLUS_010.40/zoneState.h:6-21); the four
// placement bytes are its vs_TilePosition tile.
struct RoomActorRecord {
  // The room spawn index plus one (upstream index).
  std::uint8_t spawn_slot = 0;
  std::uint8_t hidden = 0;
  std::int16_t ai_flags = 0;
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::int16_t strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t agility = 0;
  std::array<std::int16_t, kRoomActorRecordLimbs> limb_hp{};
  std::int16_t reserved_1e = 0;
  std::int32_t statuses = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t direction = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t height = 0;
};

namespace layout {
inline constexpr std::size_t kRoomActorRecordBytes = 0x28;
static_assert(sizeof(RoomActorRecord) == kRoomActorRecordBytes);
inline constexpr std::size_t kRoomActorLimbHpOffset = 0x12;
static_assert(offsetof(RoomActorRecord, limb_hp) == kRoomActorLimbHpOffset);
inline constexpr std::size_t kRoomActorStatusesOffset = 0x20;
static_assert(offsetof(RoomActorRecord, statuses) == kRoomActorStatusesOffset);
inline constexpr std::size_t kRoomActorTileXOffset = 0x24;
static_assert(offsetof(RoomActorRecord, tile_x) == kRoomActorTileXOffset);
static_assert(std::is_trivially_copyable_v<RoomActorRecord>);
}  // namespace layout
}  // namespace VS::Actors
