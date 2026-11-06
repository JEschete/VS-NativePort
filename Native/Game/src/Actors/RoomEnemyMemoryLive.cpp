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
#include "Actors/RoomEnemyMemory.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
extern "C" {
#include "Actors/Location.h"
#include "Actors/RoomActorCache.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"
}

namespace VS::Actors {
namespace {
// LiveActorStateFlags (StateFlag::kActionAnimating): an appearance
// transition is running.
constexpr std::uint32_t kFadingStateFlag = StateFlag::kActionAnimating;

static_assert(std::size(State_recentRoomEnemyStates) == kRememberedRooms);
static_assert(std::size(State_recentRoomEnemyStates[0].actorStates) ==
              kRememberedEnemiesPerRoom);
static_assert(sizeof State_recentRoomEnemyStates[0].actorStates[0] ==
              sizeof(Actors_RoomActorRecord));
static_assert(std::size(Actors_RoomActorRecord{}.limbHp) == kRememberedLimbs);
static_assert(std::size(Battle_ActorStats{}.limbs) == kRememberedLimbs);

RememberedEnemy Decode(const Actors_RoomActorRecord& record) {
  RememberedEnemy enemy;
  enemy.spawn_tag = record.spawnSlot;
  enemy.hidden = record.isHidden;
  enemy.ai_flags = record.aiFlags;
  enemy.current_hp = record.currentHp;
  enemy.max_hp = record.maxHp;
  enemy.current_mp = record.currentMp;
  enemy.max_mp = record.maxMp;
  enemy.strength = record.strength;
  enemy.intelligence = record.intelligence;
  enemy.agility = record.agility;
  std::ranges::copy(record.limbHp, enemy.limb_hp.begin());
  enemy.reserved = record.reserved1E;
  enemy.statuses = record.statuses;
  enemy.placement = {.tile_x = record.tileX,
                     .tile_layer = record.direction,
                     .tile_z = record.tileZ,
                     .facing = record.height};
  return enemy;
}

Actors_RoomActorRecord Encode(const RememberedEnemy& enemy) {
  Actors_RoomActorRecord record{};
  record.spawnSlot = enemy.spawn_tag;
  record.isHidden = enemy.hidden;
  record.aiFlags = enemy.ai_flags;
  record.currentHp = enemy.current_hp;
  record.maxHp = enemy.max_hp;
  record.currentMp = enemy.current_mp;
  record.maxMp = enemy.max_mp;
  record.strength = enemy.strength;
  record.intelligence = enemy.intelligence;
  record.agility = enemy.agility;
  std::ranges::copy(enemy.limb_hp, std::begin(record.limbHp));
  record.reserved1E = enemy.reserved;
  record.statuses = enemy.statuses;
  record.tileX = enemy.placement.tile_x;
  record.direction = enemy.placement.tile_layer;
  record.tileZ = enemy.placement.tile_z;
  record.height = enemy.placement.facing;
  return record;
}

}  // namespace

RoomEnemyMemory LoadLiveRoomEnemyMemory() {
  RoomEnemyMemory memory;
  memory.current_room = State_currentRoomStateSlot;
  for (std::size_t index = 0; index < memory.rooms.size(); ++index) {
    const State_RoomEnemyStates& saved = State_recentRoomEnemyStates[index];
    RememberedRoom& room = memory.rooms[index];
    room.recency = std::bit_cast<std::uint8_t>(saved.recency);
    room.zone = std::bit_cast<std::uint8_t>(saved.zoneId);
    room.map = saved.mapId;
    room.next_slot = saved.nextSlot;
    for (std::size_t slot = 0; slot < room.enemies.size(); ++slot) {
      Actors_RoomActorRecord record{};
      std::memcpy(&record, saved.actorStates[slot], sizeof record);
      room.enemies[slot] = Decode(record);
    }
  }
  return memory;
}

void StoreLiveRoomEnemyMemory(const RoomEnemyMemory& memory) {
  State_currentRoomStateSlot = memory.current_room;
  for (std::size_t index = 0; index < memory.rooms.size(); ++index) {
    State_RoomEnemyStates& saved = State_recentRoomEnemyStates[index];
    const RememberedRoom& room = memory.rooms[index];
    saved.recency = std::bit_cast<char>(room.recency);
    saved.zoneId = std::bit_cast<char>(room.zone);
    saved.mapId = room.map;
    saved.nextSlot = room.next_slot;
    for (std::size_t slot = 0; slot < room.enemies.size(); ++slot) {
      const Actors_RoomActorRecord record = Encode(room.enemies[slot]);
      std::memcpy(saved.actorStates[slot], &record, sizeof record);
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

RememberedPlacement PlacementOf(const Actors_TileTarget& tile) {
  return {.tile_x = static_cast<std::uint8_t>(tile.tileX),
          .tile_layer = static_cast<std::uint8_t>(tile.tileLayer),
          .tile_z = static_cast<std::uint8_t>(tile.tileZ),
          .facing = static_cast<std::uint8_t>(tile.facing)};
}

EnemyStanding StandingOf(int actor_id, const Battle_Actor& actor,
                         const Battle_ActorStats& stats) {
  EnemyStanding standing;
  standing.spawn_index = actor.roomSpawnIndex;
  standing.fading = (LiveActorStateFlags(actor_id) & kFadingStateFlag) != 0;
  standing.ai_flags = VS::Battle::Ai::PackLiveActorAiPersistenceFlags(actor_id);
  standing.current_hp = stats.currentHp;
  standing.max_hp = stats.maxHp;
  standing.current_mp = stats.currentMp;
  standing.max_mp = stats.maxMp;
  standing.strength = std::bit_cast<std::int16_t>(stats.totalStrength);
  standing.intelligence = std::bit_cast<std::int16_t>(stats.totalIntelligence);
  standing.agility = std::bit_cast<std::int16_t>(stats.totalAgility);
  for (std::size_t limb = 0; limb < standing.limb_hp.size(); ++limb) {
    standing.limb_hp[limb] = stats.limbs[limb].hp;
  }
  standing.statuses = stats.statuses;
  // The current tile (an actor or a gimmick), then the saved placement of an
  // actor record. Zero-initialized: the original left the stack word unset
  // when the slot had neither.
  Actors_ActorLocationQuery query{};
  LiveActorPlacement(actor_id, &query);
  LiveActorSavedPlacement(actor_id, std::bit_cast<u_char>(stats.movementClass),
                          &query);
  standing.placement = PlacementOf(query.location.packedTile.target);
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
  const Battle_Actor* actor = Battle_actors[actor_id];
  if (actor == nullptr || actor->roomSpawnIndex == kNoRoomSpawn) {
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

Actors_RoomActorRecord* CopyLiveRememberedEnemy(int spawn_index,
                                                Actors_RoomActorRecord* out) {
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
