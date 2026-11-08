// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/DoorMatchesLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Field/TrapsLive.hpp"
#include "Items/LootLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Effects/ActionEffects.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Field/ChestInteractionsLive.h"
#include "Field/DoorMatchList.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/InteractionFocusLive.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/TransitionsLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Palette.h"

// decomp: D_800F1910 (D_800F1910@BATTLE/BATTLE.PRG)
Field_InteractionFocus Field_interactionFocus;

// decomp: D_800F196C (D_800F196C@BATTLE/BATTLE.PRG)
u_int Field_controlMode;

// decomp: BATTLE.PRG func_8006CE70 (src/BATTLE/BATTLE.PRG/146C.c:2163)
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* target_position)
{
    int room_object_count;
    Field_MpdLockTrigger* door_records;
    Items_TreasureChest* chest_records;
    int lock_level_upper_exclusive = 4;
    int minimum_breakable_lock_level = 2;

    switch (target_position->pad & 0x1F0) {
    case 0x10:
        door_records = Field_RoomInteractiveDoors(&room_object_count);

        if ((door_records[target_position->pad & 0xF].trigger.lockState.lock.state < lock_level_upper_exclusive)
            && (door_records[target_position->pad & 0xF].trigger.lockState.lock.state >= minimum_breakable_lock_level)
            && (VS::Field::RollLiveRiskLockBreak() != 0)) {

            door_records[target_position->pad & 0xF].trigger.lockState.lock.state = 0;

            VS::Audio::LivePlaySfxCentered(0x7E, 0x3F);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x40);

        } else {
            ((void (*)(void*))Battle_QueueWorldLockedLabel)(target_position);
        }

        break;

    case 0x20:
    case 0x100:
        chest_records = VS::Items::LiveRoomTreasureChests(&room_object_count);

        if ((chest_records[target_position->pad & 0xF].lockLevel < lock_level_upper_exclusive)
            && (chest_records[target_position->pad & 0xF].lockLevel >= 2)
            && !VS::State::LiveScriptFlag(chest_records[target_position->pad & 0xF].openedFlag + 0x3C0)
            && (VS::Field::RollLiveRiskLockBreak() != 0)) {

            chest_records[target_position->pad & 0xF].lockLevel = 0;

            VS::Audio::LivePlaySfxCentered(0x7E, 0x3F);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x40);

        } else {
            ((void (*)(void*))Battle_QueueWorldLockedLabel)(target_position);
        }
        break;
    }
}

// decomp: BATTLE.PRG func_80074BAC (src/BATTLE/BATTLE.PRG/146C.c:5365)
int Field_CheckAshleyTileTriggers(int actorId, u_char* inputPosition)
{
    u_char* position = inputPosition;
    Field_MpdDoorRecord destination{};
    Actors_ActorLocation* coordinates;
    Field_MpdFloorTrigger* roomTrigger;
    const Field_MpdTileCell* tile;
    Field_DoorMatchList* doors;
    Field_MpdDoorRecord* door;
    Battle_Actor* actor;
    Battle_Actor** actorSlot;
    Battle_Actor** const actorTable = Battle_actors;
    u_int tileMask;
    int phaseValue;
    u_int floorOverride;
    int tileMode;
    int doorIndex;
    int handled = 0;

    phaseValue = VS::Actors::LiveActorStateFlags(actorId);
    tileMode = (u_int)phaseValue >> 14;
    tileMode &= 1;
    floorOverride = phaseValue & 0x70000;
    coordinates = (Actors_ActorLocation*)position;
    if (floorOverride != 0) {
        tile = VS::Field::LiveCollisionCellOrAbsent(0, 0, floorOverride >> 16);
    } else {
        tile = VS::Field::LiveCollisionCellOrAbsent(position[0],
            position[2], position[1]);
    }

    if (Field_screenTransitionStep != 0 || (phaseValue & 0x01000000)) {
        return 0;
    }

    tileMask = 0xFFFFFF;
    actorSlot = actorTable + actorId;
    actor = *actorSlot;
    if ((*(u_int*)position & tileMask) != (actor->lastTriggerTile.word & tileMask)
        || tileMode != actor->lastTriggerTile.bytes[3] || (phaseValue & 0x70000)) {
        if (Field_controlMode != 3) {
            if (tile->hasEventTrigger && Event_EvaluateRoomEventTriggersEntry(2, 0, NULL, 0) == 1) {
                VS::Event::BeginLiveQueuedCutscene();
                actor->lastTriggerTile.word = *(u_int*)position;
                actor->lastTriggerTile.bytes[3] = tileMode;
                if (tile->hasRoomTrigger) {
                    roomTrigger = Field_RoomTriggerAtTile(position[0],
                        position[2], position[1]);
                    if (roomTrigger != NULL && roomTrigger->kind == 0x10) {
                        Effects_SpawnTrapTileEffect(coordinates);
                    }
                }
                VS::Actors::ResetLiveActorRunMomentum(actorId);
                return 1;
            }

            if (tile->hasRoomTrigger && !(phaseValue & 0x504000)) {
                roomTrigger = Field_RoomTriggerAtTile(position[0],
                    position[2], position[1]);
                if (roomTrigger != NULL) {
                    if (roomTrigger->kind != 0x10) {
                        Field_BeginTrapInteractionEntry((Field_MpdLockTrigger*)roomTrigger, coordinates, actorId);
                        actor->lastTriggerTile.word = *(u_int*)position;
                        actor->lastTriggerTile.bytes[3] = tileMode;
                        return 1;
                    }
                    Effects_SpawnTrapTileEffect(coordinates);
                }
            }

            if (tile->isPlatformDocked && !(phaseValue & 0x4000)) {
                floorOverride = phaseValue & 0x70000;
                if (floorOverride != 0) {
                    if (phaseValue & 0x20000000) {
                        doors = NULL;
                    } else {
                        doors = VS::Field::TransitionLiveDoorsAtTile(0, 0, floorOverride >> 16);
                    }
                } else {
                    doors = VS::Field::TransitionLiveDoorsAtTile(position[0],
                        position[2], position[1]);
                }

                if (doors != NULL) {
                    for (doorIndex = 0; doorIndex < doors->count; ++doorIndex) {
                        door = doors->records[doorIndex];
                        if ((u_char)VS::State::LiveScriptFlag(door->requiredFlagId) >= door->requiredFlagValue) {
                            if (door->destinationZoneId == Field_zoneContext.zndId) {
                                VS::Field::LiveBeginRoomTransition(door, 0, 8);
                            } else {
                                VS::Field::LiveBeginZoneTransition(door, 0, 8);
                            }
                            VS::Actors::ResetLiveActorRunMomentum(actorId);
                            Field_SetEntranceDoorRecord((char (*)[12])doors->records[doorIndex]);
                            return 1;
                        }
                    }
                }
            }
        }

        if (!(phaseValue & 0x104000)) {
            actor->lastTriggerTile.word = *(u_int*)position;
        }
        actor->lastTriggerTile.bytes[3] = tileMode;
    }

    if (handled == 0 && tile->isPit && coordinates->position.vy > 16) {
        phaseValue = VS::Field::LiveRoomFallDestination();
        if (phaseValue != -1 && phaseValue != 0) {
            destination.destinationZoneId = phaseValue / 65536;
            destination.destinationRoomId = phaseValue - (phaseValue / 65536) * 65536;
            destination.entryTileX = destination.tileLocation.tileX = position[0];
            destination.entryTileZ = destination.tileLocation.tileZ = position[2];
            destination.tileLocation.floorLayer = position[1];
            destination.tileLocation.entryDirection = 7;
            destination.entryElevation = position[3] >> 6;
            destination.hasEntryStep = 0;
            destination.reserved06Bit15 = 0;
            if (destination.destinationZoneId == Field_zoneContext.zndId) {
                VS::Field::LiveBeginRoomTransition(&destination, 0, 8);
            } else {
                VS::Field::LiveBeginZoneTransition(&destination, 0, 8);
            }
            VS::Actors::ResetLiveActorRunMomentum(actorId);
            Field_SetEntranceDoorRecord(NULL);
            return 0;
        }

        if (actor->stats->currentHp < 21) {
            if (actor->stats->currentHp != 0) {
                ((void (*)(int arg0, int arg1, u_int arg2, int arg3))Battle_QueueActorValueLabel)(actor->stats->actorId, 0, 0x80000014, 0);
            }
            actor->stats->currentHp = 0;
            Battle_BeginActorDefeatAnimation(actor->stats->actorId);
            return 1;
        }

        actor->stats->currentHp -= 20;
        Field_StoreEntranceDoorForReload();
        VS::Field::LiveBeginRoomTransition(&Field_pendingDestinationDoor, 0, 8);
        ((void (*)(int arg0, int arg1, u_int arg2, int arg3))Battle_QueueActorValueLabel)(actor->stats->actorId, 0, 0x80000014, 0);
        VS::Actors::ResetLiveActorRunMomentum(actorId);
        Field_SetEntranceDoorRecord((char (*)[12])&Field_pendingDestinationDoor);
        return 1;
    }
    return 0;
}

}  // extern "C"
