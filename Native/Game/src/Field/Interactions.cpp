// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/DoorMatchesLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Field/TrapsLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
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
#include "Items/KeyItems.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/TextBox.h"
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

// decomp: BATTLE.PRG func_80073AA4 (src/BATTLE/BATTLE.PRG/146C.c:4823)
void Field_BeginObjectInteractionWait(void)
{
    Field_controlMode = 0xB;
    VS::Field::LivePauseFieldForObjectInteraction();
}

// decomp: BATTLE.PRG func_80073ACC (src/BATTLE/BATTLE.PRG/146C.c:4829)
void Field_EndObjectInteractionWait(void)
{
    Menu_ReleaseSceneNotification();
    VS::Field::LiveResumeFieldAfterMenu();
    VS::Field::LiveEnterFieldExplorationMode();
}

// decomp: BATTLE.PRG func_80073AFC (src/BATTLE/BATTLE.PRG/146C.c:4836)
int Field_InteractWithLockedRoomObject(Field_MpdLockTrigger* door_trigger)
{
    short interaction_target_position[4];
    int action;
    int template_;

    if (door_trigger->isOpen == 0) {
        if (door_trigger->isTriggered == 0) {
            int v = 7;
            interaction_target_position[0] = (door_trigger->trigger.tileX << v) + 0x40;
            interaction_target_position[2] = (door_trigger->trigger.tileZ << v) + 0x40;
            action = 0;

            switch (door_trigger->trigger.lockState.lock.state) {
            case 0:
                action = 1;
                break;
            case 1:
            case 3:
                if (VS::Items::CountLiveInventoryItem(door_trigger->keyOrSigil) != 0) {
                    if (door_trigger->keyOrSigil >= Items_KeyItemId_ChamomileSigil) {
                        VS::State::SetLiveScriptFlag(door_trigger->keyOrSigil + 0x1B6, 2);
                        VS::Items::ConsumeLiveMiscItem(door_trigger->keyOrSigil);
                    }
                    door_trigger->trigger.lockState.lock.state = 0;
                    action = 2;
                } else if (VS::State::LiveScriptFlag(door_trigger->keyOrSigil + 0x1B6)) {
                    door_trigger->trigger.lockState.lock.state = 0;
                    action = 3;
                }
                break;
            }

            if (action != 0) {
                if (action == 2) {
                    template_ = 7; // Unlocked with key
                    if (door_trigger->keyOrSigil >= Items_KeyItemId_ChamomileSigil) {
                        template_ = 9; // Unlocked with sigil
                    }
                    Menu_ShowSceneNotification(template_, door_trigger->keyOrSigil, 1);
                }
                VS::Actors::PlayLiveActorActionPose(0, interaction_target_position, 5);
                Field_BeginDoorToggle(door_trigger);
                return 0;
            }

            if (door_trigger->trigger.lockState.lock.state != 0) {
                switch (door_trigger->trigger.lockState.lock.state) {
                case 1:
                    template_ = 6; // Locked with key
                    if (door_trigger->keyOrSigil >= Items_KeyItemId_ChamomileSigil) {
                        template_ = 8; // Locked with sigil
                    }
                    break;
                case 2:
                    template_ = 5; // Magic lock (chest)
                    break;
                case 3:
                    template_ = 10; // Unconditionally locked
                    break;
                }
                Menu_ShowSceneNotification(template_, door_trigger->keyOrSigil, 1);
            }

            VS::Audio::LivePlaySfxCentered(0x7E, 0x65);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x66);
            VS::Actors::RestoreLiveActorIdlePose(0);
            return 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_80073D30 (src/BATTLE/BATTLE.PRG/146C.c:4909)
void Field_TriggerActorTrap(Field_MpdLockTrigger* trap_trigger, Actors_ActorLocationQuery* victim_placement, int targetActor)
{
    Effects_ActionEffectRequest request;

    Battle_actionState->selection.primaryTarget.targetsPosition = 3;
    Battle_actionState->selection.targetCount = 1;
    Battle_actionState->selection.targets[0].targetsPosition = 0;
    Battle_actionState->selection.targets[0].target.targetActor = targetActor;
    Battle_actionState->selection.targets[0].target.targetLimb =
        Battle_actors[targetActor]->stats->defaultTargetLimb;
    Battle_actionState->chainStep = 1;
    Battle_actionState->queueIndex = 0;
    Battle_actionState->rangeOrigin.vx = victim_placement->location.position.vx;
    Battle_actionState->rangeOrigin.vy = victim_placement->location.position.vy;
    Battle_actionState->rangeOrigin.vz = victim_placement->location.position.vz;
    Battle_ResolveActionTargets(trap_trigger->trigger.kind + 6, &Battle_actionState->queue[0], &Battle_actionState->selection, 1);
    request.effectId = trap_trigger->trigger.kind + 6;
    request.source.type = 5;
    request.targets[0].type = 4;
    request.count = 1;
    request.otherCount = 0;
    request.targets[0].value.actor.actor = targetActor;
    request.targets[0].value.actor.part = 0xFD;
    Effects_BeginSecondaryEffects(&request, 0);
}

// decomp: BATTLE.PRG func_80074120 (src/BATTLE/BATTLE.PRG/146C.c:5019)
int Field_WaitForChestCameraFocus(void)
{
    SVECTOR interaction_camera_target;

    interaction_camera_target.vx = (u_short)Field_interactionFocus.actor_query.location.position.vx;
    interaction_camera_target.vy = (u_short)Field_interactionFocus.actor_query.location.position.vy;
    interaction_camera_target.vz = (u_short)Field_interactionFocus.actor_query.location.position.vz;

    if ((VS::Field::LiveStepCameraLookAtTowardTarget(&interaction_camera_target, 0, 0x28) == 0)
        && (((VS::Actors::LiveActorStateFlags(0) & 0x08000000) != 0))) {
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_8007418C (src/BATTLE/BATTLE.PRG/146C.c:5034)
int Field_StepTrapCameraFocus(void)
{
    SVECTOR interaction_camera_target;

    interaction_camera_target.vx = (u_short)Field_interactionFocus.actor_query.location.position.vx;
    interaction_camera_target.vy = (u_short)Field_interactionFocus.actor_query.location.position.vy;
    interaction_camera_target.vz = (u_short)Field_interactionFocus.actor_query.location.position.vz;
    return VS::Field::LiveStepCameraLookAtTowardTarget(&interaction_camera_target, 0, 0x28);
}

// decomp: BATTLE.PRG func_8007424C (src/BATTLE/BATTLE.PRG/146C.c:5055)
int Field_IsChestAnimationReady(void)
{
    int ret = 0;
    if (VS::Actors::LiveActorStateFlags(0) & 0x08000000) {
        ret = Menu_IsSceneNotificationPending() == 0;
    }
    return ret;
}

// decomp: BATTLE.PRG func_80074374 (src/BATTLE/BATTLE.PRG/146C.c:5089)
int Field_TrackInteractionCameraToAshley(void)
{
    SVECTOR ashley_camera_target;
    short ashley_placement_words[6];
    int camera_tracking_speed;

    VS::Actors::LiveActorPlacement(0, (Actors_ActorLocationQuery*)ashley_placement_words);
    ashley_camera_target.vx = ashley_placement_words[2];
    ashley_camera_target.vz = ashley_placement_words[4];
    ashley_camera_target.vy = ashley_placement_words[3] - 0x5A;
    camera_tracking_speed = VS::Field::LiveStepCameraLookAtTowardTarget(&ashley_camera_target, 1, 0x28);

    if (VS::Actors::IsLiveAshleyTransitionPose() != 0) {
        return 0;
    }
    return camera_tracking_speed;
}

// decomp: BATTLE.PRG func_80074798 (src/BATTLE/BATTLE.PRG/146C.c:5232)
int Field_TileInFrontOfActor(Actors_ActorLocationQuery* actor_placement, u_char* output_facing_tile)
{
    int rounded_facing_or_quadrant = (actor_placement->location.position.pad + 0x200) & 0xFFF;
    if (rounded_facing_or_quadrant < 0) {
        rounded_facing_or_quadrant += 0x3FF;
    }
    output_facing_tile[0] = actor_placement->location.packedTile.target.tileX;
    output_facing_tile[2] = actor_placement->location.packedTile.target.tileZ;
    output_facing_tile[1] = actor_placement->location.packedTile.target.tileLayer;

    rounded_facing_or_quadrant = rounded_facing_or_quadrant >> 0xA;
    switch (rounded_facing_or_quadrant) {
    case 0:
        --output_facing_tile[2];
        break;
    case 1:
        --output_facing_tile[0];
        break;
    case 2:
        ++output_facing_tile[2];
        break;
    default:
        ++output_facing_tile[0];
        break;
    }
    return rounded_facing_or_quadrant;
}

// decomp: BATTLE.PRG func_80074860 (src/BATTLE/BATTLE.PRG/146C.c:5260)
int Field_TryTriggerFacingTileEvent(int actor_index)
{
    Actors_ActorLocationQuery actor_placement;
    u_char facing_tile[4];

    VS::Actors::LiveActorPlacement(actor_index, &actor_placement);
    Field_TileInFrontOfActor(&actor_placement, facing_tile);
    // BUG: arg2 should be an actor ID?
    return Event_EvaluateRoomEventTriggersEntry(4, 0, facing_tile, 1) == 1;
}

// decomp: BATTLE.PRG func_800748B8 (src/BATTLE/BATTLE.PRG/146C.c:5271)
Field_MpdLockTrigger* Field_FindFacingRoomObject(int actor_index)
{
    Actors_ActorLocationQuery actor_placement;
    u_char facing_tile[4];
    Field_MpdLockTrigger* facing_door;
    u_char* facing_tile_bytes = facing_tile;

    VS::Actors::LiveActorPlacement(actor_index, &actor_placement);
    Field_TileInFrontOfActor(&actor_placement, facing_tile);
    facing_door = Field_InteractiveDoorAtTile(facing_tile_bytes[0], facing_tile_bytes[2], facing_tile_bytes[1]);
    if (((((VS::Field::LiveFloorHeightAt((facing_tile_bytes[0] << 7) | 0x40, (facing_tile_bytes[2] << 7) | 0x40) << 0x11) >> 0x11)
             - actor_placement.location.position.vy)
            + 0x3F)
        >= 0x7FU) {
        facing_door = NULL;
    }
    return facing_door;
}

// decomp: BATTLE.PRG func_80074950 (src/BATTLE/BATTLE.PRG/146C.c:5290)
Items_TreasureChest* Field_FindFacingUnlootedChest(int actor_index)
{
    Actors_ActorLocationQuery actor_placement;
    u_char facing_tile[4];
    Items_TreasureChest* facing_chest;
    int facing_quadrant;
    u_char* facing_tile_bytes = facing_tile;

    VS::Actors::LiveActorPlacement(actor_index, &actor_placement);
    facing_quadrant = Field_TileInFrontOfActor(&actor_placement, facing_tile);
    facing_chest = VS::Items::FindLiveTreasureChestAtTile(facing_tile_bytes[0], facing_tile_bytes[2], facing_tile_bytes[1]);

    if ((facing_chest != NULL) && (facing_chest->facing == facing_quadrant)) {
        if (!VS::State::LiveScriptFlag((facing_chest->openedFlag + 0x3C0))) {
            int v = ((VS::Field::LiveFloorHeightAt((facing_tile_bytes[0] << 7) | 0x40, (facing_tile_bytes[2] << 7) | 0x40) << 0x11)
                        >> 0x11)
                  + 0x60;
            if (actor_placement.location.position.vy == v) {
                return facing_chest;
            }
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_80074BAC (src/BATTLE/BATTLE.PRG/146C.c:5365)
int Field_CheckAshleyTileTriggers(int actorId, u_char* inputPosition)
{
    u_char* position = inputPosition;
    Field_MpdDoorRecord destination;
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

// decomp: BATTLE.PRG func_800751B8 (src/BATTLE/BATTLE.PRG/146C.c:5514)
int Field_CheckActorTrapAndFallTriggers(int actor_index, Actors_ActorLocation* actor_location)
{
    Field_MpdFloorTrigger* floor_trigger;
    const Field_MpdTileCell* collision_cell;
    int trigger_tile_mode;
    u_int platform_layer_flags;
    u_int actor_state_flags;
    Battle_Actor* battle_actor;
    Actors_ActorLocation* location_alias = actor_location;
    int _[4] __attribute__((unused));

    actor_state_flags = VS::Actors::LiveActorStateFlags(actor_index);
    platform_layer_flags = actor_state_flags & 0x70000;
    trigger_tile_mode = (actor_state_flags >> 0xE);
    trigger_tile_mode &= 1;

    if (platform_layer_flags != 0) {
        collision_cell = VS::Field::LiveCollisionCellOrAbsent(0, 0, platform_layer_flags >> 0x10);
    } else {
        collision_cell = VS::Field::LiveCollisionCellOrAbsent(actor_location->packedTile.target.tileX, actor_location->packedTile.target.tileZ,
            actor_location->packedTile.target.tileLayer);
    }

    if (Field_screenTransitionStep == 0) {
        if (!(actor_state_flags & 0x01000000)) {
            battle_actor = Battle_actors[actor_index];

            if (((location_alias->packedTile.word & 0xFFFFFF) != (battle_actor->lastTriggerTile.word & 0xFFFFFF))
                || (trigger_tile_mode != battle_actor->lastTriggerTile.bytes[3]) || (actor_state_flags & 0x70000)) {
                if (Field_controlMode != 3) {
                    if (collision_cell->hasRoomTrigger) {
                        if (!(actor_state_flags & 0x16704000)) {
                            floor_trigger = Field_RoomTriggerAtTile(location_alias->packedTile.target.tileX,
                                location_alias->packedTile.target.tileZ, location_alias->packedTile.target.tileLayer);
                            if ((floor_trigger != NULL) && (floor_trigger->kind != 0x10)) {
                                Field_BeginTrapInteractionEntry((Field_MpdLockTrigger*)floor_trigger, actor_location, actor_index);
                                battle_actor->lastTriggerTile.word = location_alias->packedTile.word;
                                battle_actor->lastTriggerTile.bytes[3] = trigger_tile_mode;
                                return 3;
                            }
                        }
                    }
                }
                if (!(actor_state_flags & 0x504000)) {
                    battle_actor->lastTriggerTile.word = location_alias->packedTile.word;
                }
                battle_actor->lastTriggerTile.bytes[3] = trigger_tile_mode;
            }
            if ((collision_cell->isPit)) {
                if (actor_location->position.vy >= 0x11) {
                    Battle_actors[actor_index]->stats->currentHp = 0;
                    Battle_ResolveActorDefeat(0, actor_index, 0);
                    if (Event_EvaluateRoomEventTriggersEntry(0xC, 0, NULL, 0) == 1) {
                        return 1;
                    }
                    return 2;
                }
            }
        }
    }
    return 0;
}

}  // extern "C"
