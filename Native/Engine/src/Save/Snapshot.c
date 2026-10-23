// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/RoomState.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Field/Camera.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "Save/Snapshot.h"
#include "State/Ashley.h"
#include "State/Location.h"
#include "State/Progress.h"

// decomp: SLUS_010.40 func_80042CA0 (src/SLUS_010.40/main.c:8373)
void Save_MarkAshleySnapshotValid(void) { State_ashleySnapshot.stats.isPending = 1; }

// decomp: SLUS_010.40 func_80042CB0 (src/SLUS_010.40/main.c:8375)
void Save_CaptureAshleySnapshot(void)
{
    Actors_ActorLocationQuery sp10;
    int j;
    int i;
    int var_s1;
    int var_t3;
    int new_var;
    int var_t4;
    signed char actionsLearned;
    Battle_ActorStats* temp_t0;
    State_AshleySnapshot* temp_t5;
    int v0;
    int v0_2;
    int v1;
    int v1_2;

    State_savedEntryDoor = Field_pendingDestinationDoor;
    Actors_ActorPlacement(0, &sp10);

    State_savedEntryDoor.entryTileX = sp10.location.packedTile.target.tileX;
    State_savedEntryDoor.entryTileZ = sp10.location.packedTile.target.tileZ;
    State_savedEntryDoor.tileLocation.entryDirection = sp10.location.packedTile.target.tileLayer;
    State_savedEntryDoor.entryElevation = sp10.location.packedTile.target.facing >> 6;
    State_savedEntryDoor.requiredFlagId = Field_snapshotDoorFlagSource.requiredFlagId;

    for (var_s1 = 0; var_s1 < 32; ++var_s1) {
        for (actionsLearned = 0, i = 0; i < 8; ++i) {
            actionsLearned =
                (*(int*)&actionsLearned * 2) | Battle_actionDefinitions[var_s1 * 8 + i].unlocked;
        }
        State_learnedActionFlags[var_s1] = actionsLearned;
    }
    temp_t0 = Battle_characterState->stats;
    temp_t5 = &State_ashleySnapshot;
    temp_t5->stats.currentHp = temp_t0->currentHp;
    temp_t5->stats.maxHp = temp_t0->maxHp;
    temp_t5->stats.currentMp = temp_t0->currentMp;
    temp_t5->stats.maxMp = temp_t0->maxMp;
    temp_t5->stats.strength = temp_t0->strength;
    temp_t5->stats.totalStrength = temp_t0->totalStrength;
    temp_t5->stats.intelligence = temp_t0->intelligence;
    temp_t5->stats.totalIntelligence = temp_t0->totalIntelligence;
    temp_t5->stats.agility = temp_t0->agility;
    temp_t5->stats.totalAgility = temp_t0->totalAgility;
    temp_t5->stats.risk = temp_t0->risk;
    temp_t5->stats.actorFlags = temp_t0->actionRecoveryTimer;
    temp_t5->stats.statuses = temp_t0->statuses;

    for (i = 0; i < 8; ++i) {
        temp_t5->stats.statusTimers[i] = temp_t0->statusTimers[i];
    }

    var_t4 = 0;
    var_t3 = 0;

    for (i = 0; i < 6; ++i) {
        temp_t5->stats.limbs[i].hp = temp_t0->limbs[i].hp;
        temp_t5->stats.limbs[i].maxHp = temp_t0->limbs[i].maxHp;

        for (j = 0; j < 4; ++j) {
            temp_t5->stats.limbs[i].types[j] = temp_t0->limbs[i].types[j];
        }

        for (j = 0; j < 8; ++j) {
            temp_t5->stats.limbs[i].affinities[j] = temp_t0->limbs[i].affinities[j];
        }
    }

    temp_t5->stats.weaponDrawn = Battle_characterState->weaponDrawn;
    temp_t5->stats.cameraPitch = Field_sphericalCamera.delta.pitch;
    temp_t5->stats.cameraDistance = Field_sphericalCamera.initialDistance;

    for (var_s1 = 0; var_s1 < 16; ++var_s1) {
        Actors_StoreRoomActorState(var_s1, 0);
    }
    Field_StoreEntranceDoorForReload();
}

