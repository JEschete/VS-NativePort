// Hand-maintained (tools/port/owned.csv).
// The remembered enemies of the recently visited rooms (BATTLE.PRG
// 146C.c:783-882).
#include "Actors/RoomEnemyMemory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace VS::Actors {
namespace {
// The record tag a spawn index is stored under; it wraps like the original's
// byte store, so index 255 takes the unused tag 0.
std::uint8_t SpawnTag(int spawn_index) {
  return static_cast<std::uint8_t>(spawn_index + 1);
}

// The original compares the record's tag byte with the int spawn_index + 1,
// so index 255 never matches.
bool HoldsSpawn(const RememberedEnemy& enemy, int spawn_index) {
  return static_cast<int>(enemy.spawn_tag) == spawn_index + 1;
}
}  // namespace

RememberedRoom* CurrentRememberedRoom(RoomEnemyMemory& memory) {
  if (memory.current_room < 0 ||
      static_cast<std::size_t>(memory.current_room) >= memory.rooms.size()) {
    return nullptr;
  }
  return &memory.rooms[static_cast<std::size_t>(memory.current_room)];
}

const RememberedRoom* CurrentRememberedRoom(const RoomEnemyMemory& memory) {
  if (memory.current_room < 0 ||
      static_cast<std::size_t>(memory.current_room) >= memory.rooms.size()) {
    return nullptr;
  }
  return &memory.rooms[static_cast<std::size_t>(memory.current_room)];
}

void ClearRoomEnemyMemory(RoomEnemyMemory& memory) { memory = {}; }

void ForgetRoomEnemies(RoomEnemyMemory& memory, int map, int zone) {
  for (RememberedRoom& room : memory.rooms) {
    if (static_cast<int>(room.map) == map &&
        static_cast<int>(room.zone) == zone) {
      room.enemies = {};
    }
  }
}

RememberedEnemy* FindRememberedEnemy(RoomEnemyMemory& memory, int spawn_index) {
  RememberedRoom* room = CurrentRememberedRoom(memory);
  if (room == nullptr) {
    return nullptr;
  }
  RememberedEnemy* found = nullptr;
  for (RememberedEnemy& enemy : room->enemies) {
    if (HoldsSpawn(enemy, spawn_index)) {
      found = &enemy;
    }
  }
  return found;
}

RememberedEnemy* AcquireRememberedEnemy(RoomEnemyMemory& memory,
                                        int spawn_index) {
  RememberedRoom* room = CurrentRememberedRoom(memory);
  if (room == nullptr) {
    return nullptr;
  }
  const auto match = std::ranges::find_if(
      room->enemies, [spawn_index](const RememberedEnemy& enemy) {
        return HoldsSpawn(enemy, spawn_index);
      });
  if (match != room->enemies.end()) {
    return &*match;
  }
  if (room->next_slot >= room->enemies.size()) {
    return nullptr;
  }
  RememberedEnemy* taken = &room->enemies[room->next_slot];
  ++room->next_slot;
  if (room->next_slot >= room->enemies.size()) {
    room->next_slot = 0;
  }
  return taken;
}

bool RememberEnemy(RoomEnemyMemory& memory, const EnemyStanding& enemy,
                   std::uint8_t hidden) {
  RememberedEnemy* record = AcquireRememberedEnemy(memory, enemy.spawn_index);
  if (record == nullptr) {
    return false;
  }
  record->spawn_tag = SpawnTag(enemy.spawn_index);
  record->current_hp = enemy.fading ? std::int16_t{1} : enemy.current_hp;
  record->max_hp = enemy.max_hp;
  record->current_mp = enemy.current_mp;
  record->max_mp = enemy.max_mp;
  record->strength = enemy.strength;
  record->intelligence = enemy.intelligence;
  record->agility = enemy.agility;
  record->limb_hp = enemy.limb_hp;
  record->statuses = enemy.statuses;
  record->hidden = hidden;
  record->ai_flags = enemy.ai_flags;
  record->placement = enemy.placement;
  return true;
}

bool IsCurrentRememberedRoom(const RoomEnemyMemory& memory, int zone, int map) {
  const RememberedRoom* room = CurrentRememberedRoom(memory);
  return room != nullptr && static_cast<int>(room->zone) == zone &&
         static_cast<int>(room->map) == map;
}
}  // namespace VS::Actors
