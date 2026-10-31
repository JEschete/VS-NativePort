// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
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
    Actors_ActorLocationQuery ashley_placement;
    int j;
    int i;
    int action_byte_or_actor_slot;
    int var_t3;
    int new_var;
    int var_t4;
    signed char actionsLearned;
    Battle_ActorStats* ashley_stats;
    State_AshleySnapshot* ashley_snapshot;
    int v0;
    int v0_2;
    int v1;
    int v1_2;

    State_savedEntryDoor = Field_pendingDestinationDoor;
    Actors_ActorPlacement(0, &ashley_placement);

    State_savedEntryDoor.entryTileX = ashley_placement.location.packedTile.target.tileX;
    State_savedEntryDoor.entryTileZ = ashley_placement.location.packedTile.target.tileZ;
    State_savedEntryDoor.tileLocation.entryDirection = ashley_placement.location.packedTile.target.tileLayer;
    State_savedEntryDoor.entryElevation = ashley_placement.location.packedTile.target.facing >> 6;
    State_savedEntryDoor.requiredFlagId = Field_snapshotDoorFlagSource.requiredFlagId;

    for (action_byte_or_actor_slot = 0; action_byte_or_actor_slot < 32; ++action_byte_or_actor_slot) {
        for (actionsLearned = 0, i = 0; i < 8; ++i) {
            actionsLearned =
                (*(int*)&actionsLearned * 2) | Battle_actionDefinitions[action_byte_or_actor_slot * 8 + i].unlocked;
        }
        State_learnedActionFlags[action_byte_or_actor_slot] = actionsLearned;
    }
    ashley_stats = Battle_characterState->stats;
    ashley_snapshot = &State_ashleySnapshot;
    ashley_snapshot->stats.currentHp = ashley_stats->currentHp;
    ashley_snapshot->stats.maxHp = ashley_stats->maxHp;
    ashley_snapshot->stats.currentMp = ashley_stats->currentMp;
    ashley_snapshot->stats.maxMp = ashley_stats->maxMp;
    ashley_snapshot->stats.strength = ashley_stats->strength;
    ashley_snapshot->stats.totalStrength = ashley_stats->totalStrength;
    ashley_snapshot->stats.intelligence = ashley_stats->intelligence;
    ashley_snapshot->stats.totalIntelligence = ashley_stats->totalIntelligence;
    ashley_snapshot->stats.agility = ashley_stats->agility;
    ashley_snapshot->stats.totalAgility = ashley_stats->totalAgility;
    ashley_snapshot->stats.risk = ashley_stats->risk;
    ashley_snapshot->stats.actorFlags = ashley_stats->actionRecoveryTimer;
    ashley_snapshot->stats.statuses = ashley_stats->statuses;

    for (i = 0; i < 8; ++i) {
        ashley_snapshot->stats.statusTimers[i] = ashley_stats->statusTimers[i];
    }

    var_t4 = 0;
    var_t3 = 0;

    for (i = 0; i < 6; ++i) {
        ashley_snapshot->stats.limbs[i].hp = ashley_stats->limbs[i].hp;
        ashley_snapshot->stats.limbs[i].maxHp = ashley_stats->limbs[i].maxHp;

        for (j = 0; j < 4; ++j) {
            ashley_snapshot->stats.limbs[i].types[j] = ashley_stats->limbs[i].types[j];
        }

        for (j = 0; j < 8; ++j) {
            ashley_snapshot->stats.limbs[i].affinities[j] = ashley_stats->limbs[i].affinities[j];
        }
    }

    ashley_snapshot->stats.weaponDrawn = Battle_characterState->weaponDrawn;
    ashley_snapshot->stats.cameraPitch = Field_sphericalCamera.delta.pitch;
    ashley_snapshot->stats.cameraDistance = Field_sphericalCamera.initialDistance;

    for (action_byte_or_actor_slot = 0; action_byte_or_actor_slot < 16; ++action_byte_or_actor_slot) {
        Actors_StoreRoomActorState(action_byte_or_actor_slot, 0);
    }
    Field_StoreEntranceDoorForReload();
}

}  // extern "C"
