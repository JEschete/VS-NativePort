#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

// The remembered enemies of the four most recently visited rooms (BATTLE.PRG
// 146C.c:783-882). Leaving a room records each enemy's vitals, limbs,
// statuses, AI flags and placement under its room spawn index; returning to
// the room restores them. RoomEnemyMemoryLive binds the saved room records
// (State_recentRoomEnemyStates and State_currentRoomStateSlot).
namespace VS::Actors {
inline constexpr std::size_t kRememberedRooms = 4;
inline constexpr std::size_t kRememberedEnemiesPerRoom = 8;
inline constexpr std::size_t kRememberedLimbs = 6;

// The tile, tile layer and facing an enemy is restored at.
struct RememberedPlacement {
  std::uint8_t tile_x = 0;
  std::uint8_t tile_layer = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t facing = 0;

  friend bool operator==(const RememberedPlacement&,
                         const RememberedPlacement&) = default;
};

struct RememberedEnemy {
  // The room spawn index plus one; 0 marks an unused record.
  std::uint8_t spawn_tag = 0;
  std::uint8_t hidden = 0;
  std::int16_t ai_flags = 0;
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::int16_t strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t agility = 0;
  std::array<std::int16_t, kRememberedLimbs> limb_hp{};
  // Never written by the originals; kept so a record round-trips unchanged.
  std::int16_t reserved = 0;
  std::int32_t statuses = 0;
  RememberedPlacement placement;

  friend bool operator==(const RememberedEnemy&,
                         const RememberedEnemy&) = default;
};

struct RememberedRoom {
  // Non-zero once the room has been visited; the room loader ages it.
  std::uint8_t recency = 0;
  std::uint8_t zone = 0;
  std::uint16_t map = 0;
  // The record the next new enemy takes once every record is in use.
  std::uint32_t next_slot = 0;
  std::array<RememberedEnemy, kRememberedEnemiesPerRoom> enemies{};

  friend bool operator==(const RememberedRoom&,
                         const RememberedRoom&) = default;
};

struct RoomEnemyMemory {
  std::array<RememberedRoom, kRememberedRooms> rooms{};
  // The room the current map's enemies are recorded in.
  std::int32_t current_room = 0;

  friend bool operator==(const RoomEnemyMemory&,
                         const RoomEnemyMemory&) = default;
};

// What RememberEnemy records for one live enemy.
struct EnemyStanding {
  std::uint8_t spawn_index = 0;
  // An appearance transition (a death or disappearance fade) is running: the
  // enemy is remembered with 1 HP.
  bool fading = false;
  std::int16_t ai_flags = 0;
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::int16_t strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t agility = 0;
  std::array<std::int16_t, kRememberedLimbs> limb_hp{};
  std::int32_t statuses = 0;
  RememberedPlacement placement;
};

// The current room, or null when current_room is outside the four rooms.
[[nodiscard]] RememberedRoom* CurrentRememberedRoom(RoomEnemyMemory& memory);
[[nodiscard]] const RememberedRoom* CurrentRememberedRoom(
    const RoomEnemyMemory& memory);

// func_80069DBC (146C.c:783): forgets every room and selects room 0.
void ClearRoomEnemyMemory(RoomEnemyMemory& memory);
// func_80069DEC (146C.c:789): clears the enemy records of each room that
// remembers `map` in `zone`, keeping its recency, place and next slot.
void ForgetRoomEnemies(RoomEnemyMemory& memory, int map, int zone);
// func_80069E80 (146C.c:800): the current room's record for `spawn_index`;
// with duplicates, the last one. Null when there is none or the current room
// is invalid.
[[nodiscard]] RememberedEnemy* FindRememberedEnemy(RoomEnemyMemory& memory,
                                                   int spawn_index);
// func_80069EF0 (146C.c:813): the current room's first record for
// `spawn_index`, else the record at next_slot, advancing next_slot through
// the eight records. Null (the room unchanged) when the current room is
// invalid or next_slot is outside the records; the original wrote past them.
[[nodiscard]] RememberedEnemy* AcquireRememberedEnemy(RoomEnemyMemory& memory,
                                                      int spawn_index);
// func_80069FC4 (146C.c:840): records `enemy` in the current room. A fading
// enemy keeps 1 HP. Returns false, with the memory unchanged, when
// AcquireRememberedEnemy has no record to give.
bool RememberEnemy(RoomEnemyMemory& memory, const EnemyStanding& enemy,
                   std::uint8_t hidden);
// Whether the current room remembers `map` in `zone` (the spawn checks at
// 146C.c:6650 and 6759); false when the current room is invalid.
[[nodiscard]] bool IsCurrentRememberedRoom(const RoomEnemyMemory& memory,
                                           int zone, int map);
}  // namespace VS::Actors
