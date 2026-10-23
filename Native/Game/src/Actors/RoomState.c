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
#include "Battle/Ai/ActorState.h"
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
void* Actors_FindCachedRoomActorState(int arg0)
{
    int i;
    void* var_a3 = NULL;

    for (i = 0; i < 8; ++i) {
        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i][0] == (arg0 + 1)) {
            var_a3 = &State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i];
        }
    }
    return var_a3;
}

// decomp: BATTLE.PRG func_80069EF0 (src/BATTLE/BATTLE.PRG/146C.c:813)
void* Actors_AcquireCachedRoomActorState(int arg0)
{
    int i;
    void* var_a3;

    var_a3 = NULL;

    for (i = 0; i < 8; ++i) {
        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i][0] == (arg0 + 1)) {
            var_a3 = &State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i];
            break;
        }
    }

    if (var_a3 == NULL) {
        i = State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot;
        var_a3 = &State_recentRoomEnemyStates[State_currentRoomStateSlot].actorStates[i];
        ++State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot;

        if (State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot >= 8) {
            State_recentRoomEnemyStates[State_currentRoomStateSlot].nextSlot = 0;
        }
    }

    return var_a3;
}

// decomp: BATTLE.PRG func_80069FC4 (src/BATTLE/BATTLE.PRG/146C.c:840)
void Actors_StoreRoomActorState(int arg0, int arg1)
{
    Actors_ActorLocationQuery sp10;
    int i;
    Actors_RoomActorRecordWrite* temp_v0;
    Battle_ActorStats* temp_s1;
    Battle_Actor* temp_s0 = Battle_actors[arg0];

    if ((temp_s0 == NULL) || (temp_s0->roomSpawnIndex == 0x80)) {
        return;
    }

    temp_s1 = temp_s0->stats;
    temp_v0 = Actors_AcquireCachedRoomActorState(temp_s0->roomSpawnIndex);
    temp_v0->spawnSlot = temp_s0->roomSpawnIndex + 1;
    temp_v0->currentHp = temp_s1->currentHp;
    temp_v0->maxHp = temp_s1->maxHp;
    temp_v0->currentMp = temp_s1->currentMp;
    temp_v0->maxMp = temp_s1->maxMp;
    temp_v0->strength = temp_s1->totalStrength;
    temp_v0->intelligence = temp_s1->totalIntelligence;
    temp_v0->agility = temp_s1->totalAgility;

    for (i = 0; i < 6; ++i) {
        temp_v0->limbHp[i] = temp_s1->limbs[i].hp;
    }

    temp_v0->statuses = temp_s1->statuses;

    if (Actors_ActorStateFlags(arg0) & 0x01000000) {
        temp_v0->isHidden = arg1;
        temp_v0->currentHp = 1;
    } else {
        temp_v0->isHidden = arg1;
    }

    temp_v0->aiFlags = Battle_Ai_PackActorAiPersistenceFlags(arg0);

    Actors_ActorPlacement(arg0, &sp10);
    Actors_ActorSavedPlacement(arg0, temp_s1->movementClass, &sp10);

    temp_v0->placement = sp10.location.packedTile.word;
}

