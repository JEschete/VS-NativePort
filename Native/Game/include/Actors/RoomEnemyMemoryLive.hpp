#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Actors/RoomEnemyMemory.hpp"

struct Actors_RoomActorRecord;

// Bindings from the saved room records (State_recentRoomEnemyStates and
// State_currentRoomStateSlot, which the save files and the room loader also
// read and write) to the typed room enemy memory (RoomEnemyMemory.cpp). Each
// call reads the records, runs the typed operation and writes them back.
// Refusals are reported on the "actors" channel.
namespace VS::Actors {
// The saved room records as the typed memory, and back (the room load's
// enemy cache, 146C.c:15190).
[[nodiscard]] RoomEnemyMemory LoadLiveRoomEnemyMemory();
void StoreLiveRoomEnemyMemory(const RoomEnemyMemory& memory);
// func_80069DBC (146C.c:783).
void ClearLiveRoomEnemyMemory();
// func_80069DEC (146C.c:789).
void ForgetLiveRoomEnemies(int map, int zone);
// func_80069FC4 (146C.c:840): remembers the room enemy in actor slot
// `actor_id`. An empty slot and an actor without a room spawn (index 0x80)
// are skipped; an id outside the actor record table is refused.
void StoreLiveRoomActorState(int actor_id, int hidden);
// Whether the current saved room is `map` in `zone` (146C.c:6650, 6759).
[[nodiscard]] bool IsLiveRememberedRoom(int zone, int map);
// Copies the current room's record for `spawn_index` (func_80069E80) into
// `out` and returns it, or returns null when there is none.
Actors_RoomActorRecord* CopyLiveRememberedEnemy(int spawn_index,
                                                Actors_RoomActorRecord* out);
// The remembered hidden flag of `spawn_index` in the current room, or
// `fallback` when it has no record (146C.c:15205).
[[nodiscard]] std::uint8_t LiveRememberedEnemyHidden(int spawn_index,
                                                     std::uint8_t fallback);
// Sets the remembered hidden flag of `spawn_index` in the current room when
// it has a record (146C.c:15125).
void SetLiveRememberedEnemyHidden(int spawn_index, std::uint8_t hidden);
}  // namespace VS::Actors
