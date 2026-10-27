// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/RoomActorCache.h"
#include "Actors/RoomState.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Damage.h"
#include "Core/Memory/WordCopy.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"

// decomp: BATTLE.PRG func_80069DBC (src/BATTLE/BATTLE.PRG/146C.c:783)
void Actors_ClearRoomActorStateCache(void)
{
    State_currentRoomStateSlot = 0;
    Core_Memory_ZeroAlignedWords(State_recentRoomEnemyStates, sizeof State_recentRoomEnemyStates);
}

// decomp: BATTLE.PRG func_80069DEC (src/BATTLE/BATTLE.PRG/146C.c:789)
void Actors_ClearCachedRoomActorStates(int mapId, int zndId)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if ((State_recentRoomEnemyStates[i].mapId == mapId) && (State_recentRoomEnemyStates[i].zoneId == zndId)) {
            Core_Memory_ZeroAlignedWords(State_recentRoomEnemyStates[i].actorStates, sizeof State_recentRoomEnemyStates[i].actorStates);
        }
    }
}

// decomp: BATTLE.PRG func_80069E80 (src/BATTLE/BATTLE.PRG/146C.c:800)
void* Actors_FindCachedRoomActorState(int spawn_index)
{
    int i;
    void* cached_state = NULL;

    for (i = 0; i < 8; ++i) {
        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i][0] == (spawn_index + 1)) {
            cached_state = &State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i];
        }
    }
    return cached_state;
}

// decomp: BATTLE.PRG func_80069EF0 (src/BATTLE/BATTLE.PRG/146C.c:813)
void* Actors_AcquireCachedRoomActorState(int spawn_index)
{
    int i;
    void* cached_state;

    cached_state = NULL;

    for (i = 0; i < 8; ++i) {
        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i][0] == (spawn_index + 1)) {
            cached_state = &State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i];
            break;
        }
    }

    if (cached_state == NULL) {
        i = State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot;
        cached_state = &State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i];
        ++State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot;

        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot >= 8) {
            State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot = 0;
        }
    }

    return cached_state;
}

// decomp: BATTLE.PRG func_80069FC4 (src/BATTLE/BATTLE.PRG/146C.c:840)
void Actors_StoreRoomActorState(int actor_id, int hidden_in_cache)
{
    Actors_ActorLocationQuery placement;
    int i;
    Actors_RoomActorRecordWrite* cached_state;
    Battle_ActorStats* stats;
    Battle_Actor* actor = Battle_actors[actor_id];

    if ((actor == NULL) || (actor->roomSpawnIndex == 0x80)) {
        return;
    }

    stats = actor->stats;
    cached_state = Actors_AcquireCachedRoomActorState(actor->roomSpawnIndex);
    cached_state->spawnSlot = actor->roomSpawnIndex + 1;
    cached_state->currentHp = stats->currentHp;
    cached_state->maxHp = stats->maxHp;
    cached_state->currentMp = stats->currentMp;
    cached_state->maxMp = stats->maxMp;
    cached_state->strength = stats->totalStrength;
    cached_state->intelligence = stats->totalIntelligence;
    cached_state->agility = stats->totalAgility;

    for (i = 0; i < 6; ++i) {
        cached_state->limbHp[i] = stats->limbs[i].hp;
    }

    cached_state->statuses = stats->statuses;

    if (Actors_ActorStateFlags(actor_id) & 0x01000000) {
        cached_state->isHidden = hidden_in_cache;
        cached_state->currentHp = 1;
    } else {
        cached_state->isHidden = hidden_in_cache;
    }

    cached_state->aiFlags = Battle_Ai_PackActorAiPersistenceFlags(actor_id);

    Actors_ActorPlacement(actor_id, &placement);
    Actors_ActorSavedPlacement(actor_id, stats->movementClass, &placement);

    cached_state->placement = placement.location.packedTile.word;
}

