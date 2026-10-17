// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Audio/Sfx.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#include "DiscQueue.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Event/Cutscene.h"
#include "Event/Triggers.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Interaction.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/Transitions.h"
#include "Field/Traps.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/KeyItems.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Palette.h"
#include "State/Flags.h"
#include "State/Settings.h"
#include "State/Timers.h"

// decomp: D_800F1910 (D_800F1910@BATTLE/BATTLE.PRG)
Field_InteractionFocus Field_interactionFocus;

// decomp: D_800F196C (D_800F196C@BATTLE/BATTLE.PRG)
u_int Field_controlMode;

// decomp: BATTLE.PRG func_8006CE70 (src/BATTLE/BATTLE.PRG/146C.c:2163)
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* arg0)
{
    int sp10;
    Field_MpdLockTrigger* temp_v0;
    Items_TreasureChest* temp_v0_2;
    int new_var2 = 4;
    int new_var3 = 2;

    switch (arg0->pad & 0x1F0) {
    case 0x10:
        temp_v0 = Field_RoomInteractiveDoors(&sp10);

        if ((temp_v0[arg0->pad & 0xF].trigger.lockState.lock.state < new_var2)
            && (temp_v0[arg0->pad & 0xF].trigger.lockState.lock.state >= new_var3)
            && (Field_RollRiskLockBreak() != 0)) {

            temp_v0[arg0->pad & 0xF].trigger.lockState.lock.state = 0;

            Audio_PlaySfxCentered(0x7E, 0x3F);
            Audio_PlaySfxCentered(0x7E, 0x40);

        } else {
            ((void (*)(void*))Battle_QueueWorldLockedLabel)(arg0);
        }

        break;

    case 0x20:
    case 0x100:
        temp_v0_2 = Items_RoomTreasureChests(&sp10);

        if ((temp_v0_2[arg0->pad & 0xF].lockLevel < new_var2)
            && (temp_v0_2[arg0->pad & 0xF].lockLevel >= 2)
            && !State_ScriptFlag(temp_v0_2[arg0->pad & 0xF].openedFlag + 0x3C0)
            && (Field_RollRiskLockBreak() != 0)) {

            temp_v0_2[arg0->pad & 0xF].lockLevel = 0;

            Audio_PlaySfxCentered(0x7E, 0x3F);
            Audio_PlaySfxCentered(0x7E, 0x40);

        } else {
            ((void (*)(void*))Battle_QueueWorldLockedLabel)(arg0);
        }
        break;
    }
}

// decomp: BATTLE.PRG func_80073AA4 (src/BATTLE/BATTLE.PRG/146C.c:4823)
void Field_BeginObjectInteractionWait(void)
{
    Field_controlMode = 0xB;
    Field_PauseFieldForObjectInteraction();
}

// decomp: BATTLE.PRG func_80073ACC (src/BATTLE/BATTLE.PRG/146C.c:4829)
void Field_EndObjectInteractionWait(void)
{
    Menu_ReleaseSceneNotification();
    Field_ResumeFieldAfterMenu();
    Field_EnterFieldExplorationMode();
}

// decomp: BATTLE.PRG func_80073AFC (src/BATTLE/BATTLE.PRG/146C.c:4836)
int Field_InteractWithLockedRoomObject(Field_MpdLockTrigger* arg0)
{
    short sp10[4];
    int action;
    int template;

    if (arg0->isOpen == 0) {
        if (arg0->isTriggered == 0) {
            int v = 7;
            sp10[0] = (arg0->trigger.tileX << v) + 0x40;
            sp10[2] = (arg0->trigger.tileZ << v) + 0x40;
            action = 0;

            switch (arg0->trigger.lockState.lock.state) {
            case 0:
                action = 1;
                break;
            case 1:
            case 3:
                if (Items_InventoryItemCount(arg0->keyOrSigil) != 0) {
                    if (arg0->keyOrSigil >= Items_KeyItemId_ChamomileSigil) {
                        State_SetScriptFlag(arg0->keyOrSigil + 0x1B6, 2);
                        Items_ConsumeMiscItem(arg0->keyOrSigil);
                    }
                    arg0->trigger.lockState.lock.state = 0;
                    action = 2;
                } else if (State_ScriptFlag(arg0->keyOrSigil + 0x1B6)) {
                    arg0->trigger.lockState.lock.state = 0;
                    action = 3;
                }
                break;
            }

            if (action != 0) {
                if (action == 2) {
                    template = 7; // Unlocked with key
                    if (arg0->keyOrSigil >= Items_KeyItemId_ChamomileSigil) {
                        template = 9; // Unlocked with sigil
                    }
                    Menu_ShowSceneNotification(template, arg0->keyOrSigil, 1);
                }
                ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, sp10, 5);
                Field_BeginDoorToggle(arg0);
                return 0;
            }

            if (arg0->trigger.lockState.lock.state != 0) {
                switch (arg0->trigger.lockState.lock.state) {
                case 1:
                    template = 6; // Locked with key
                    if (arg0->keyOrSigil >= Items_KeyItemId_ChamomileSigil) {
                        template = 8; // Locked with sigil
                    }
                    break;
                case 2:
                    template = 5; // Magic lock (chest)
                    break;
                case 3:
                    template = 10; // Unconditionally locked
                    break;
                }
                Menu_ShowSceneNotification(template, arg0->keyOrSigil, 1);
            }

            Audio_PlaySfxCentered(0x7E, 0x65);
            Audio_PlaySfxCentered(0x7E, 0x66);
            Actors_RestoreActorIdlePose(0);
            return 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG func_80073D30 (src/BATTLE/BATTLE.PRG/146C.c:4909)
void Field_TriggerActorTrap(Field_MpdLockTrigger* arg0, Actors_ActorLocationQuery* arg1, int targetActor)
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
    Battle_actionState->rangeOrigin.vx = arg1->location.position.vx;
    Battle_actionState->rangeOrigin.vy = arg1->location.position.vy;
    Battle_actionState->rangeOrigin.vz = arg1->location.position.vz;
    Battle_ResolveActionTargets(arg0->trigger.kind + 6, &Battle_actionState->queue[0], &Battle_actionState->selection, 1);
    request.effectId = arg0->trigger.kind + 6;
    request.source.type = 5;
    request.targets[0].type = 4;
    request.count = 1;
    request.otherCount = 0;
    request.targets[0].value.actor.actor = targetActor;
    request.targets[0].value.actor.part = 0xFD;
    Effects_BeginSecondaryEffects(&request, 0);
}

// decomp: BATTLE.PRG func_80073E30 (src/BATTLE/BATTLE.PRG/146C.c:4935)
void Field_TryOpenChest(Field_MpdLockTrigger* arg0, int arg1)
{
    short sp10[4];
    int var_s1;
    int shift = 7;

    sp10[0] = (arg0->trigger.tileX << shift) + 0x40;
    sp10[2] = (arg0->trigger.tileZ << shift) + 0x40;
    var_s1 = 0;

    switch (arg0->keyOrSigil) {
    case 0:
        var_s1 = 1;
        break;
    case 1:
    case 3:
        if (!State_ScriptFlag(arg0->isOpen + 0x1B6)) {
            if (Items_InventoryItemCount(arg0->isOpen) == 0) {
                break;
            }
            State_SetScriptFlag(arg0->isOpen + 0x1B6, 1);
            if (arg0->isOpen >= 0x1D2) {
                Items_ConsumeMiscItem(arg0->isOpen);
            }
        }
        arg0->keyOrSigil = 0;
        Menu_ShowSceneNotification(4, arg0->isOpen, 1);
        var_s1 = 1;
        break;
    }
    if (var_s1 != 0) {
        ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, sp10, 2);
        Field_BeginSwitchToggle(arg0);
        return;
    }
    ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, sp10, 3);
}

// decomp: BATTLE.PRG func_80073F7C (src/BATTLE/BATTLE.PRG/146C.c:4973)
void Field_BeginTrapInteraction(Field_MpdLockTrigger* arg0, Actors_ActorLocation* arg1, int arg2)
{
    int i;

    Field_controlMode = 8;
    Battle_actionFlowMode = 1;
    Field_StorePreferredActionCameraDistance();
    Field_interactionFocus.triggerRecord = arg0;
    Field_interactionFocus.actorQuery.location = *arg1;
    Field_interactionFocus.actorQuery.actorIndex = arg2;
    for (i = 0; i < 16; ++i) {
        Actors_RestoreActorIdlePose(i);
    }
    if (Input_settings.information != 0) {
        Menu_ShowSceneNotification(0xB, arg0->trigger.kind + 6, 1);
    }
    Battle_actionState->chainStep = 0;
    Battle_actionState->selection.actionId = 0;
}

// decomp: BATTLE.PRG func_80074050 (src/BATTLE/BATTLE.PRG/146C.c:4993)
void Field_BeginChestInteraction(Field_MpdLockTrigger* arg0, int arg1)
{
    short sp10[4];
    int i;
    int var_v0;

    Field_controlMode = 9;
    Battle_actionFlowMode = 1;
    Field_StorePreferredActionCameraDistance();
    Field_interactionFocus.triggerRecord = arg0;
    Actors_ActorPlacement(arg1, &Field_interactionFocus.actorQuery);
    Field_interactionFocus.actorQuery.actorIndex = arg1;

    for (i = 0; i < 16; ++i) {
        Actors_RestoreActorIdlePose(i);
    }

    var_v0 = arg0->trigger.tileX;
    sp10[0] = (var_v0 << 7) + 0x40;
    var_v0 = arg0->trigger.tileZ;
    sp10[2] = (var_v0 << 7) + 0x40;
    ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, sp10, 1);
    Battle_actionState->chainStep = 0;
    Battle_actionState->selection.actionId = 0;
}

// decomp: BATTLE.PRG func_80074120 (src/BATTLE/BATTLE.PRG/146C.c:5019)
int Field_WaitForChestCameraFocus(void)
{
    SVECTOR sp10;

    sp10.vx = (u_short)Field_interactionFocus.actorQuery.location.position.vx;
    sp10.vy = (u_short)Field_interactionFocus.actorQuery.location.position.vy;
    sp10.vz = (u_short)Field_interactionFocus.actorQuery.location.position.vz;

    if ((Field_StepCameraLookAtTowardTarget(&sp10, 0, 0x28) == 0)
        && (((Actors_ActorStateFlags(0) & 0x08000000) != 0))) {
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_8007418C (src/BATTLE/BATTLE.PRG/146C.c:5034)
int Field_StepTrapCameraFocus(void)
{
    SVECTOR sp10;

    sp10.vx = (u_short)Field_interactionFocus.actorQuery.location.position.vx;
    sp10.vy = (u_short)Field_interactionFocus.actorQuery.location.position.vy;
    sp10.vz = (u_short)Field_interactionFocus.actorQuery.location.position.vz;
    return Field_StepCameraLookAtTowardTarget(&sp10, 0, 0x28);
}

// decomp: BATTLE.PRG func_800741D4 (src/BATTLE/BATTLE.PRG/146C.c:5044)
void Field_DispatchTrapOrChestInteraction(void)
{
    Battle_actionFlowMode = 2;
    if (Field_controlMode == 8) {
        State_tickSpeed = 4;
        Field_TriggerActorTrap(Field_interactionFocus.triggerRecord, &Field_interactionFocus.actorQuery, Field_interactionFocus.actorQuery.actorIndex);
        return;
    }
    Field_TryOpenChest(Field_interactionFocus.triggerRecord, Field_interactionFocus.actorQuery.actorIndex);
}

// decomp: BATTLE.PRG func_8007424C (src/BATTLE/BATTLE.PRG/146C.c:5055)
int Field_IsChestAnimationReady(void)
{
    int ret = 0;
    if (Actors_ActorStateFlags(0) & 0x08000000) {
        ret = Menu_IsSceneNotificationPending() == 0;
    }
    return ret;
}

// decomp: BATTLE.PRG func_800742A4 (src/BATTLE/BATTLE.PRG/146C.c:5066)
void Field_FinishChestInteraction(void)
{
    short sp10[4];
    int v;

    Battle_actionFlowMode = 4;
    State_tickSpeed = 2;
    v = 7;
    sp10[0] = (Field_interactionFocus.triggerRecord->trigger.tileX << v) + 0x40;
    sp10[2] = (Field_interactionFocus.triggerRecord->trigger.tileZ << v) + 0x40;
    ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, sp10, 4);
    Menu_ReleaseSceneNotification();
}

// decomp: BATTLE.PRG func_80074314 (src/BATTLE/BATTLE.PRG/146C.c:5080)
void Field_FinishTrapInteraction(void)
{
    Battle_actionFlowMode = 4;
    State_tickSpeed = 2;
    Menu_ReleaseSceneNotification();
    Field_SetRoomTriggerEnabled(&Field_interactionFocus.triggerRecord->trigger, 1);
    Battle_Ai_MarkAiTileDynamicObstacle(Field_interactionFocus.triggerRecord->trigger.tileX, Field_interactionFocus.triggerRecord->trigger.tileZ);
}

// decomp: BATTLE.PRG func_80074374 (src/BATTLE/BATTLE.PRG/146C.c:5089)
int Field_TrackInteractionCameraToAshley(void)
{
    SVECTOR sp10;
    short sp18[6];
    int temp_s0;

    Actors_ActorPlacement(0, (Actors_ActorLocationQuery*)sp18);
    sp10.vx = sp18[2];
    sp10.vz = sp18[4];
    sp10.vy = sp18[3] - 0x5A;
    temp_s0 = Field_StepCameraLookAtTowardTarget(&sp10, 1, 0x28);

    if (Actors_IsAshleyTransitionPose() != 0) {
        return 0;
    }
    return temp_s0;
}

// decomp: BATTLE.PRG func_800743E0 (src/BATTLE/BATTLE.PRG/146C.c:5107)
void Field_OpenChestContents()
{
    int temp_v1 = Field_interactionFocus.triggerRecord->keyOrSigil;
    int arg1;

    Battle_actionFlowMode = 3;

    if (temp_v1 != 0) {
        switch (temp_v1) {
        case 1:
            arg1 = 2;
            break;
        case 2:
            arg1 = 1;
            break;
        case 3:
            arg1 = 3;
            break;
        }
        Menu_ShowSceneNotification(arg1, Field_interactionFocus.triggerRecord->isOpen, 1);
        Field_PauseFieldForInteractionDialog();
    } else {
        short temp_v1_2 = Field_interactionFocus.triggerRecord->linkedLockIndex;
        if (temp_v1_2 < 0x40) {
            Menu_Framework_OpenLootEntryMenu((Items_BattleLoot*)(Field_interactionFocus.triggerRecord + 1));
        } else {
            Menu_Framework_OpenChestStorageMenu(temp_v1_2 - 0x40);
        }
        Field_PauseFieldForMenu();
    }
}

// decomp: BATTLE.PRG func_800744B8 (src/BATTLE/BATTLE.PRG/146C.c:5139)
int Field_UpdateChestContentsSelection(void)
{
    Menu_Framework_ActionSelection* temp_v0;

    if (Field_interactionFocus.triggerRecord->keyOrSigil != 0) {
        if (Menu_IsSceneNotificationPending() != 0) {
            return 1;
        }
        Field_ResumeFieldAfterMenu();
        return 0;
    }
    Menu_Framework_UpdateMenuOverlay();
    temp_v0 = Menu_Framework_ActionSelectionState();
    if (temp_v0->menuStatus == 1) {
        return 1;
    }
    if (temp_v0->selectionKind != 0) {
        State_SetScriptFlag((Field_interactionFocus.triggerRecord->linkedLockIndex + 0x3C0), 1);
    } else {
        Field_BeginSwitchToggle(Field_interactionFocus.triggerRecord);
    }
    Menu_Framework_ReleaseMenuRows();
    Field_ResumeFieldAfterMenu();
    return 0;
}

// decomp: BATTLE.PRG func_80074798 (src/BATTLE/BATTLE.PRG/146C.c:5232)
int Field_TileInFrontOfActor(Actors_ActorLocationQuery* arg0, u_char* arg1)
{
    int var_v1 = (arg0->location.position.pad + 0x200) & 0xFFF;
    if (var_v1 < 0) {
        var_v1 += 0x3FF;
    }
    arg1[0] = arg0->location.packedTile.target.tileX;
    arg1[2] = arg0->location.packedTile.target.tileZ;
    arg1[1] = arg0->location.packedTile.target.tileLayer;

    var_v1 = var_v1 >> 0xA;
    switch (var_v1) {
    case 0:
        --arg1[2];
        break;
    case 1:
        --arg1[0];
        break;
    case 2:
        ++arg1[2];
        break;
    default:
        ++arg1[0];
        break;
    }
    return var_v1;
}

// decomp: BATTLE.PRG func_80074860 (src/BATTLE/BATTLE.PRG/146C.c:5260)
int Field_TryTriggerFacingTileEvent(int arg0)
{
    Actors_ActorLocationQuery sp10;
    u_char sp20[4];

    Actors_ActorPlacement(arg0, &sp10);
    Field_TileInFrontOfActor(&sp10, sp20);
    // BUG: arg2 should be an actor ID?
    return Event_EvaluateRoomEventTriggers(4, 0, sp20, 1) == 1;
}

// decomp: BATTLE.PRG func_800748B8 (src/BATTLE/BATTLE.PRG/146C.c:5271)
Field_MpdLockTrigger* Field_FindFacingRoomObject(int arg0)
{
    Actors_ActorLocationQuery sp10;
    u_char sp20[4];
    Field_MpdLockTrigger* temp_s1;
    u_char* s0 = sp20;

    Actors_ActorPlacement(arg0, &sp10);
    Field_TileInFrontOfActor(&sp10, sp20);
    temp_s1 = Field_InteractiveDoorAtTile(s0[0], s0[2], s0[1]);
    if (((((Field_FloorHeightAt((s0[0] << 7) | 0x40, (s0[2] << 7) | 0x40) << 0x11) >> 0x11)
             - sp10.location.position.vy)
            + 0x3F)
        >= 0x7FU) {
        temp_s1 = NULL;
    }
    return temp_s1;
}

// decomp: BATTLE.PRG func_80074950 (src/BATTLE/BATTLE.PRG/146C.c:5290)
Items_TreasureChest* Field_FindFacingUnlootedChest(int arg0)
{
    Actors_ActorLocationQuery sp10;
    u_char sp20[4];
    Items_TreasureChest* temp_v0;
    int temp_s2;
    u_char* s1 = sp20;

    Actors_ActorPlacement(arg0, &sp10);
    temp_s2 = Field_TileInFrontOfActor(&sp10, sp20);
    temp_v0 = Items_TreasureChestAtTile(s1[0], s1[2], s1[1]);

    if ((temp_v0 != NULL) && (temp_v0->facing == temp_s2)) {
        if (!State_ScriptFlag((temp_v0->openedFlag + 0x3C0))) {
            int v = ((Field_FloorHeightAt((s1[0] << 7) | 0x40, (s1[2] << 7) | 0x40) << 0x11)
                        >> 0x11)
                  + 0x60;
            if (sp10.location.position.vy == v) {
                return temp_v0;
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
    Field_MpdTileCell* tile;
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

    phaseValue = Actors_ActorStateFlags(actorId);
    tileMode = (u_int)phaseValue >> 14;
    tileMode &= 1;
    floorOverride = phaseValue & 0x70000;
    coordinates = (Actors_ActorLocation*)position;
    if (floorOverride != 0) {
        tile = Field_CollisionCellAt(0, 0, floorOverride >> 16);
    } else {
        tile = Field_CollisionCellAt(position[0],
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
            if (tile->hasEventTrigger && Event_EvaluateRoomEventTriggers(2, 0, NULL, 0) == 1) {
                Event_BeginQueuedCutscene();
                actor->lastTriggerTile.word = *(u_int*)position;
                actor->lastTriggerTile.bytes[3] = tileMode;
                if (tile->hasRoomTrigger) {
                    roomTrigger = Field_RoomTriggerAtTile(position[0],
                        position[2], position[1]);
                    if (roomTrigger != NULL && roomTrigger->kind == 0x10) {
                        Effects_SpawnTrapTileEffect(coordinates);
                    }
                }
                Actors_ResetActorRunMomentum(actorId);
                return 1;
            }

            if (tile->hasRoomTrigger && !(phaseValue & 0x504000)) {
                roomTrigger = Field_RoomTriggerAtTile(position[0],
                    position[2], position[1]);
                if (roomTrigger != NULL) {
                    if (roomTrigger->kind != 0x10) {
                        Field_BeginTrapInteraction((Field_MpdLockTrigger*)roomTrigger, coordinates, actorId);
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
                        doors = Field_TransitionDoorsAtTile(0, 0, floorOverride >> 16);
                    }
                } else {
                    doors = Field_TransitionDoorsAtTile(position[0],
                        position[2], position[1]);
                }

                if (doors != NULL) {
                    for (doorIndex = 0; doorIndex < doors->count; ++doorIndex) {
                        door = doors->records[doorIndex];
                        if ((u_char)State_ScriptFlag(door->requiredFlagId) >= door->requiredFlagValue) {
                            if (door->destinationZoneId == Field_zoneContext.zndId) {
                                Field_BeginRoomTransition(door, 0, 8);
                            } else {
                                Field_BeginZoneTransition(door, 0, 8);
                            }
                            Actors_ResetActorRunMomentum(actorId);
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
        phaseValue = Field_RoomFallDestination();
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
                Field_BeginRoomTransition(&destination, 0, 8);
            } else {
                Field_BeginZoneTransition(&destination, 0, 8);
            }
            Actors_ResetActorRunMomentum(actorId);
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
        Field_BeginRoomTransition(&Field_pendingDestinationDoor, 0, 8);
        ((void (*)(int arg0, int arg1, u_int arg2, int arg3))Battle_QueueActorValueLabel)(actor->stats->actorId, 0, 0x80000014, 0);
        Actors_ResetActorRunMomentum(actorId);
        Field_SetEntranceDoorRecord((char (*)[12])&Field_pendingDestinationDoor);
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800751B8 (src/BATTLE/BATTLE.PRG/146C.c:5514)
int Field_CheckActorTrapAndFallTriggers(int arg0, Actors_ActorLocation* arg1)
{
    Field_MpdFloorTrigger* temp_v0_2;
    Field_MpdTileCell* temp_s4;
    int temp_s3;
    u_int temp_a2;
    u_int temp_v0;
    Battle_Actor* temp_s0;
    Actors_ActorLocation* s2 = arg1;
    int _[4] __attribute__((unused));

    temp_v0 = Actors_ActorStateFlags(arg0);
    temp_a2 = temp_v0 & 0x70000;
    temp_s3 = (temp_v0 >> 0xE);
    temp_s3 &= 1;

    if (temp_a2 != 0) {
        temp_s4 = Field_CollisionCellAt(0, 0, temp_a2 >> 0x10);
    } else {
        temp_s4 = Field_CollisionCellAt(arg1->packedTile.target.tileX, arg1->packedTile.target.tileZ,
            arg1->packedTile.target.tileLayer);
    }

    if (Field_screenTransitionStep == 0) {
        if (!(temp_v0 & 0x01000000)) {
            temp_s0 = Battle_actors[arg0];

            if (((s2->packedTile.word & 0xFFFFFF) != (temp_s0->lastTriggerTile.word & 0xFFFFFF))
                || (temp_s3 != temp_s0->lastTriggerTile.bytes[3]) || (temp_v0 & 0x70000)) {
                if (Field_controlMode != 3) {
                    if (temp_s4->hasRoomTrigger) {
                        if (!(temp_v0 & 0x16704000)) {
                            temp_v0_2 = Field_RoomTriggerAtTile(s2->packedTile.target.tileX,
                                s2->packedTile.target.tileZ, s2->packedTile.target.tileLayer);
                            if ((temp_v0_2 != NULL) && (temp_v0_2->kind != 0x10)) {
                                Field_BeginTrapInteraction((Field_MpdLockTrigger*)temp_v0_2, arg1, arg0);
                                temp_s0->lastTriggerTile.word = s2->packedTile.word;
                                temp_s0->lastTriggerTile.bytes[3] = temp_s3;
                                return 3;
                            }
                        }
                    }
                }
                if (!(temp_v0 & 0x504000)) {
                    temp_s0->lastTriggerTile.word = s2->packedTile.word;
                }
                temp_s0->lastTriggerTile.bytes[3] = temp_s3;
            }
            if ((temp_s4->isPit)) {
                if (arg1->position.vy >= 0x11) {
                    Battle_actors[arg0]->stats->currentHp = 0;
                    Battle_ResolveActorDefeat(0, arg0, 0);
                    if (Event_EvaluateRoomEventTriggers(0xC, 0, NULL, 0) == 1) {
                        return 1;
                    }
                    return 2;
                }
            }
        }
    }
    return 0;
}

