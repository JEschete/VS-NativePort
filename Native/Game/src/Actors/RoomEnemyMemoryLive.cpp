// Hand-maintained (tools/port/owned.csv).
// Live binding for the room enemy memory (RoomEnemyMemory.cpp).
#include "Actors/RoomEnemyMemoryLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "Actors/ActorFlags.hpp"
#include "Actors/ActorRoster.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/Location.hpp"
#include "Actors/RoomActorCache.hpp"
#include "Actors/RoomEnemyMemory.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Damage.hpp"
#include "State/RoomCache.hpp"
#include "State/RoomMemory.hpp"

namespace VS::Actors {
namespace {
// LiveActorStateFlags (StateFlag::kActionAnimating): an appearance
// transition is running.
constexpr std::uint32_t kFadingStateFlag = StateFlag::kActionAnimating;

static_assert(std::size(VS::State::recentRoomEnemyStates) == kRememberedRooms);
static_assert(std::size(VS::State::recentRoomEnemyStates[0].actor_states) ==
              kRememberedEnemiesPerRoom);
static_assert(sizeof VS::State::recentRoomEnemyStates[0].actor_states[0] ==
              sizeof(VS::Actors::RoomActorRecord));
static_assert(std::size(VS::Actors::RoomActorRecord{}.limb_hp) ==
              kRememberedLimbs);
static_assert(std::size(VS::Battle::ActorStats{}.limbs) == kRememberedLimbs);

RememberedEnemy Decode(const VS::Actors::RoomActorRecord& record) {
  RememberedEnemy enemy;
  enemy.spawn_tag = record.spawn_slot;
  enemy.hidden = record.hidden;
  enemy.ai_flags = record.ai_flags;
  enemy.current_hp = record.current_hp;
  enemy.max_hp = record.max_hp;
  enemy.current_mp = record.current_mp;
  enemy.max_mp = record.max_mp;
  enemy.strength = record.strength;
  enemy.intelligence = record.intelligence;
  enemy.agility = record.agility;
  std::ranges::copy(record.limb_hp, enemy.limb_hp.begin());
  enemy.reserved = record.reserved_1e;
  enemy.statuses = record.statuses;
  enemy.placement = {.tile_x = record.tile_x,
                     .tile_layer = record.surface,
                     .tile_z = record.tile_z,
                     .facing = record.facing};
  return enemy;
}

VS::Actors::RoomActorRecord Encode(const RememberedEnemy& enemy) {
  VS::Actors::RoomActorRecord record{};
  record.spawn_slot = enemy.spawn_tag;
  record.hidden = enemy.hidden;
  record.ai_flags = enemy.ai_flags;
  record.current_hp = enemy.current_hp;
  record.max_hp = enemy.max_hp;
  record.current_mp = enemy.current_mp;
  record.max_mp = enemy.max_mp;
  record.strength = enemy.strength;
  record.intelligence = enemy.intelligence;
  record.agility = enemy.agility;
  std::ranges::copy(enemy.limb_hp, std::begin(record.limb_hp));
  record.reserved_1e = enemy.reserved;
  record.statuses = enemy.statuses;
  record.tile_x = enemy.placement.tile_x;
  record.surface = enemy.placement.tile_layer;
  record.tile_z = enemy.placement.tile_z;
  record.facing = enemy.placement.facing;
  return record;
}

}  // namespace

RoomEnemyMemory LoadLiveRoomEnemyMemory() {
  RoomEnemyMemory memory;
  memory.current_room = VS::State::currentRoomStateSlot;
  for (std::size_t index = 0; index < memory.rooms.size(); ++index) {
    const VS::State::RoomEnemyStates& saved =
        VS::State::recentRoomEnemyStates[index];
    RememberedRoom& room = memory.rooms[index];
    room.recency = saved.recency;
    room.zone = saved.zone_id;
    room.map = saved.map_id;
    room.next_slot = saved.next_slot;
    for (std::size_t slot = 0; slot < room.enemies.size(); ++slot) {
      VS::Actors::RoomActorRecord record{};
      std::memcpy(&record, saved.actor_states[slot], sizeof record);
      room.enemies[slot] = Decode(record);
    }
  }
  return memory;
}

void StoreLiveRoomEnemyMemory(const RoomEnemyMemory& memory) {
  VS::State::currentRoomStateSlot = memory.current_room;
  for (std::size_t index = 0; index < memory.rooms.size(); ++index) {
    VS::State::RoomEnemyStates& saved = VS::State::recentRoomEnemyStates[index];
    const RememberedRoom& room = memory.rooms[index];
    saved.recency = room.recency;
    saved.zone_id = room.zone;
    saved.map_id = room.map;
    saved.next_slot = room.next_slot;
    for (std::size_t slot = 0; slot < room.enemies.size(); ++slot) {
      const VS::Actors::RoomActorRecord record = Encode(room.enemies[slot]);
      std::memcpy(saved.actor_states[slot], &record, sizeof record);
    }
  }
}

namespace {
// The current room's record for `spawn_index`, reporting an invalid current
// room (the originals indexed past the four rooms).
RememberedEnemy* FindInCurrentRoom(RoomEnemyMemory& memory, int spawn_index,
                                   const char* operation) {
  if (CurrentRememberedRoom(memory) == nullptr) {
    WarnActorRefused(operation, "the current saved room is outside the four");
    return nullptr;
  }
  return FindRememberedEnemy(memory, spawn_index);
}

RememberedPlacement PlacementOf(const VS::Actors::TileTarget& tile) {
  return {.tile_x = static_cast<std::uint8_t>(tile.tile_x),
          .tile_layer = static_cast<std::uint8_t>(tile.tile_layer),
          .tile_z = static_cast<std::uint8_t>(tile.tile_z),
          .facing = static_cast<std::uint8_t>(tile.facing)};
}

EnemyStanding StandingOf(int actor_id, const VS::Battle::Actor& actor,
                         const VS::Battle::ActorStats& stats) {
  EnemyStanding standing;
  standing.spawn_index = actor.room_spawn_index;
  standing.fading = (LiveActorStateFlags(actor_id) & kFadingStateFlag) != 0;
  standing.ai_flags = VS::Battle::Ai::PackLiveActorAiPersistenceFlags(actor_id);
  standing.current_hp = stats.current_hp;
  standing.max_hp = stats.max_hp;
  standing.current_mp = stats.current_mp;
  standing.max_mp = stats.max_mp;
  standing.strength = std::bit_cast<std::int16_t>(stats.total_strength);
  standing.intelligence = std::bit_cast<std::int16_t>(stats.total_intelligence);
  standing.agility = std::bit_cast<std::int16_t>(stats.total_agility);
  for (std::size_t limb = 0; limb < standing.limb_hp.size(); ++limb) {
    standing.limb_hp[limb] = stats.limbs[limb].hp;
  }
  standing.statuses = stats.statuses;
  // The current tile (an actor or a gimmick), then the saved placement of an
  // actor record. Zero-initialized: the original left the stack word unset
  // when the slot had neither.
  VS::Actors::ActorLocationQuery query{};
  LiveActorPlacement(actor_id, &query);
  LiveActorSavedPlacement(
      actor_id, std::bit_cast<std::uint8_t>(stats.movement_class), &query);
  standing.placement = PlacementOf(query.location.packed_tile.target);
  return standing;
}
}  // namespace

void ClearLiveRoomEnemyMemory() {
  RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  ClearRoomEnemyMemory(memory);
  StoreLiveRoomEnemyMemory(memory);
}

void ForgetLiveRoomEnemies(int map, int zone) {
  RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  ForgetRoomEnemies(memory, map, zone);
  StoreLiveRoomEnemyMemory(memory);
}

void StoreLiveRoomActorState(int actor_id, int hidden) {
  constexpr const char* kOperation = "StoreLiveRoomActorState";
  if (!IsActorTableId(actor_id)) {
    WarnActorRefused(kOperation,
                     "the actor id is outside the actor record table");
    return;
  }
  const VS::Battle::Actor* actor = VS::Battle::actors[actor_id];
  if (actor == nullptr || actor->room_spawn_index == kNoRoomSpawn) {
    return;
  }
  if (actor->stats == nullptr) {
    WarnActorRefused(kOperation, "the actor has no stats");
    return;
  }
  const EnemyStanding standing = StandingOf(actor_id, *actor, *actor->stats);
  RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  if (CurrentRememberedRoom(memory) == nullptr) {
    WarnActorRefused(kOperation, "the current saved room is outside the four");
    return;
  }
  if (!RememberEnemy(memory, standing, static_cast<std::uint8_t>(hidden))) {
    WarnActorRefused(kOperation, "the saved room's next record is outside it");
    return;
  }
  StoreLiveRoomEnemyMemory(memory);
}

bool IsLiveRememberedRoom(int zone, int map) {
  const RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  if (CurrentRememberedRoom(memory) == nullptr) {
    WarnActorRefused("IsLiveRememberedRoom",
                     "the current saved room is outside the four");
    return false;
  }
  return IsCurrentRememberedRoom(memory, zone, map);
}

VS::Actors::RoomActorRecord* CopyLiveRememberedEnemy(
    int spawn_index, VS::Actors::RoomActorRecord* out) {
  constexpr const char* kOperation = "CopyLiveRememberedEnemy";
  if (out == nullptr) {
    WarnActorRefused(kOperation, "no output record");
    return nullptr;
  }
  RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  const RememberedEnemy* enemy =
      FindInCurrentRoom(memory, spawn_index, kOperation);
  if (enemy == nullptr) {
    return nullptr;
  }
  *out = Encode(*enemy);
  return out;
}

std::uint8_t LiveRememberedEnemyHidden(int spawn_index, std::uint8_t fallback) {
  RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  const RememberedEnemy* enemy =
      FindInCurrentRoom(memory, spawn_index, "LiveRememberedEnemyHidden");
  return enemy != nullptr ? enemy->hidden : fallback;
}

void SetLiveRememberedEnemyHidden(int spawn_index, std::uint8_t hidden) {
  RoomEnemyMemory memory = LoadLiveRoomEnemyMemory();
  RememberedEnemy* enemy =
      FindInCurrentRoom(memory, spawn_index, "SetLiveRememberedEnemyHidden");
  if (enemy == nullptr) {
    return;
  }
  enemy->hidden = hidden;
  StoreLiveRoomEnemyMemory(memory);
}
}  // namespace VS::Actors
