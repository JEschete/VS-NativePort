// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/BattleMode.h"
#include "Battle/Rewards.h"
#include "Battle/StatusEffects.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Copy.h"
#include "Event/Actors.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/Doors.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: D_800F4B70 (D_800F4B70@BATTLE/BATTLE.PRG)
char Event_scriptControlledActors[17];

// decomp: D_800F4C2C (D_800F4C2C@BATTLE/BATTLE.PRG)
char Event_skipState;

// decomp: BATTLE.PRG func_800B7428 (src/BATTLE/BATTLE.PRG/4A0A8.c:967)
intptr_t Event_ScriptResetActorGroupState(u_char* instruction, short thread_index)
{
    Event_ActorSelection actor_selection;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(instruction + 1), &actor_selection);
    while (1) {
        int actor_id = Event_NextScriptSelectedActor(&actor_selection);
        if (actor_id == 1) {
            break;
        }
        Actors_InterruptActorAction(actor_id, instruction[3]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B7490 (src/BATTLE/BATTLE.PRG/4A0A8.c:983)
intptr_t Event_ScriptStoreActorTile(u_char* instruction, short thread_index)
{
    u_char position[12];
    u_short actor = Core_Memory_ReadUnalignedShort(instruction + 1);
    u_short flag = Core_Memory_ReadUnalignedShort(instruction + 3);

    if (actor & 0x1000) {
        actor &= 0x1F;
        if (Actors_ActorAtSlot(actor) == NULL) {
            State_SetScriptFlag(flag, 0xFF);
            return 0;
        }
    } else {
        actor = Actors_FindActorByModelId(actor);
        if (actor == 1) {
            State_SetScriptFlag(flag, 0xFF);
            return 0;
        }
    }
    Actors_ActorPlacement(actor, position);
    State_SetScriptFlag(flag, position[0]);
    State_SetScriptFlag(flag + 1, position[1]);
    State_SetScriptFlag(flag + 2, position[2]);
    return 0;
}

// decomp: BATTLE.PRG func_800B7574 (src/BATTLE/BATTLE.PRG/4A0A8.c:1009)
intptr_t Event_ScriptSpawnActor(u_char* instruction, short thread_index)
{
    u_char spawn_placement[12];

    spawn_placement[0] = instruction[5];
    spawn_placement[1] = instruction[6];
    spawn_placement[2] = instruction[7];
    spawn_placement[3] = instruction[8];
    Actors_QueueScriptedActorSpawn(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)),
        Core_Memory_ReadUnalignedShort(instruction + 3), (Field_ActorPlacement*)(spawn_placement));
    return 0;
}

// decomp: BATTLE.PRG func_800B75F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1022)
intptr_t Event_ScriptNavigateActorToTile(u_char* instruction, short thread_index)
{
    SVECTOR destination_position;
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    destination_position.vx = (instruction[3] << 7) + 0x40;
    destination_position.vy = 0;
    destination_position.vz = (instruction[5] << 7) + 0x40;
    if (Event_skipState == 2) {
        destination_position.pad = -0x8000;
        Actors_TranslateActorToAtRate(actor_id, &destination_position, 0);
    } else {
        Actors_BeginScriptedActorPath(actor_id, &destination_position);
    }
    Actors_ClearActorMovementModes(actor_id);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B76A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1040)
intptr_t Event_ScriptPlayActorAnimation(u_char* instruction, short thread_index)
{
    Actors_PlayActorAnimation(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)),
        Event_animationBankBases[instruction[5] >> 6] + instruction[3], instruction[4], instruction[5] & 0x3F);
    return 0;
}

// decomp: BATTLE.PRG func_800B770C (src/BATTLE/BATTLE.PRG/4A0A8.c:1047)
intptr_t Event_ScriptSetActorGroupFlags(u_char* instruction, short thread_index)
{
    Event_ActorSelection actor_selection;
    int actor_id;
    int equipment_slot1_visibility;
    int equipment_slot0_visibility;
    int actor_state_flags;
    u_int flag_options;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(instruction + 1), &actor_selection);

    while (1) {
        actor_id = Event_NextScriptSelectedActor(&actor_selection);
        if (actor_id == 1) {
            break;
        }
        actor_state_flags = Actors_ActorStateFlags(actor_id);
        equipment_slot0_visibility = actor_state_flags & 0x800;
        equipment_slot1_visibility = actor_state_flags & 0x1000;
        flag_options = instruction[4];
        if (!(flag_options & 2)) {
            equipment_slot0_visibility = (flag_options & 1) << 0xB;
        }
        if (!(flag_options & 8)) {
            equipment_slot1_visibility = (flag_options << 0xA) & 0x1000;
        }
        Actors_SetActorDetached(actor_id, (flag_options >> 5) & 1);
        Actors_SetActorDisplayFlags(actor_id, equipment_slot0_visibility | equipment_slot1_visibility | instruction[3]);
        Actors_ResetActorWeaponVisibilityBlend(actor_id);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B77DC (src/BATTLE/BATTLE.PRG/4A0A8.c:1080)
intptr_t Event_ScriptSetActorSoundMix(u_char* instruction, short thread_index)
{
    Actors_ModelEffectSlot sound_overrides;
    int actor_selector;

    sound_overrides.kind = instruction[3];
    sound_overrides.level = instruction[4];
    sound_overrides.flags = instruction[5];
    sound_overrides.parameter = instruction[6];
    actor_selector = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    if (actor_selector == 0x2001) {
        actor_selector = -1;
    }
    Actors_SetActorSoundOverrides(actor_selector, &sound_overrides);
    return 0;
}

// decomp: BATTLE.PRG func_800B784C (src/BATTLE/BATTLE.PRG/4A0A8.c:1097)
intptr_t Event_ScriptWalkActorToTile(u_char* instruction, short thread_index)
{
    u_char destination_tile[10];
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    destination_tile[0] = instruction[3];
    destination_tile[1] = instruction[4];
    destination_tile[2] = instruction[5];
    destination_tile[3] = instruction[6];
    if (Event_skipState == 2) {
        Actors_MoveActorToTileAtRate(actor_id, (Actors_TileTarget*)destination_tile, 0, 0);
    } else {
        Actors_MoveActorToTileAtRate(actor_id, (Actors_TileTarget*)destination_tile,
            instruction[7] != 0xFF ? instruction[7] : -1, instruction[8] != 0xFF ? instruction[8] : -1);
    }
    Actors_ClearActorMovementModes(actor_id);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B792C (src/BATTLE/BATTLE.PRG/4A0A8.c:1116)
intptr_t Event_ScriptWalkActorToWorldPos(u_char* instruction, short thread_index)
{
    Actors_ActorLocationQuery destination_location;

    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    int x_operand = Core_Memory_ReadUnalignedShort(instruction + 3) >> 1;
    short y_operand = Core_Memory_ReadUnalignedShort(instruction + 5);
    short z_operand = Core_Memory_ReadUnalignedShort(instruction + 7);

    if (instruction[3] & 1) {
        Actors_ActorPlacement(actor_id, &destination_location);
        applyVector(&destination_location.location.position, x_operand, y_operand, z_operand, +=);
    } else {
        setVector(&destination_location.location.position, x_operand, y_operand, z_operand);
    }
    if (instruction[9] != 0xFF) {
        char v = 0x10;
        destination_location.location.position.pad = instruction[9] * v;
    } else {
        destination_location.location.position.pad = -0x8000;
    }
    if (Event_skipState == 2) {
        Actors_MoveActorToPositionAtRate(actor_id, &destination_location.location.position, 0, 0);
    } else {
        Actors_MoveActorToPositionAtRate(actor_id, &destination_location.location.position, instruction[10] != 0xFF ? instruction[10] : -1,
            instruction[11] != 0xFF ? instruction[11] : -1);
    }
    Actors_ClearActorMovementModes(actor_id);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7A98 (src/BATTLE/BATTLE.PRG/4A0A8.c:1148)
intptr_t Event_ScriptSlideActorToTile(u_char* instruction, short thread_index)
{
    u_char destination_tile[16];
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    destination_tile[0] = instruction[3];
    destination_tile[1] = instruction[4];
    destination_tile[2] = instruction[5];
    destination_tile[3] = 0xFF;
    if (Event_skipState == 2) {
        Actors_TranslateActorToTileAtRate(actor_id, (Actors_TileTarget*)destination_tile, 0);
    } else {
        Actors_TranslateActorToTileAtRate(
            actor_id, (Actors_TileTarget*)destination_tile, instruction[6] != 0xFF ? instruction[6] : -1);
    }
    Actors_ClearActorMovementModes(actor_id);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7B58 (src/BATTLE/BATTLE.PRG/4A0A8.c:1167)
intptr_t Event_ScriptSlideActorToWorldPos(u_char* instruction, short thread_index)
{
    Actors_ActorLocationQuery destination_location;
    short y_operand;
    short z_operand;
    int x_operand;
    u_short actor_id;

    actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    x_operand = Core_Memory_ReadUnalignedShort(instruction + 3) >> 1;
    y_operand = Core_Memory_ReadUnalignedShort(instruction + 5);
    z_operand = Core_Memory_ReadUnalignedShort(instruction + 7);

    if (instruction[3] & 1) {
        Actors_ActorPlacement(actor_id, &destination_location);
        applyVector(&destination_location.location.position, x_operand, y_operand, z_operand, +=);
    } else {
        setVector(&destination_location.location.position, x_operand, y_operand, z_operand);
    }

    destination_location.location.position.pad = -0x8000;

    if (Event_skipState == 2) {
        Actors_TranslateActorToAtRate(actor_id, &destination_location.location.position, 0);
    } else {
        Actors_TranslateActorToAtRate(actor_id, &destination_location.location.position, instruction[9] != 0xFF ? instruction[9] : -1);
    }

    Actors_ClearActorMovementModes(actor_id);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7C90 (src/BATTLE/BATTLE.PRG/4A0A8.c:1200)
intptr_t Event_ScriptSetActorAttachment(u_char* instruction, short thread_index)
{
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    u_int attachment_reference = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 3)) & 0xFFFF;
    if (attachment_reference >= 0x2000) {
        Actors_ReleaseActorAttachmentLock(actor_id, attachment_reference);
    } else {
        Actors_AttachActorToParentBone(actor_id, attachment_reference, instruction[5], instruction[6]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B7D10 (src/BATTLE/BATTLE.PRG/4A0A8.c:1212)
intptr_t Event_ScriptNavigateActorToWorldPos(u_char* instruction, short thread_index)
{
    SVECTOR destination_position;
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1) & 0xFFFF);
    destination_position.vx = Core_Memory_ReadUnalignedShort(instruction + 3);
    destination_position.vy = 0;
    destination_position.vz = Core_Memory_ReadUnalignedShort(instruction + 5);
    if (Event_skipState == 2) {
        destination_position.pad = -0x8000;
        Actors_TranslateActorToAtRate(actor_id, &destination_position, 0);
    } else {
        Actors_BeginScriptedActorPath(actor_id, &destination_position);
    }
    Actors_ClearActorMovementModes(actor_id);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7DC4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1230)
intptr_t Event_ScriptTurnActorToFacing(u_char* instruction, short thread_index)
{
    Actors_ActorLocationQuery actor_location;
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    short turn_rate = instruction[5] != 0xFF ? instruction[5] : -1;
    int yaw_delta;

    if (instruction[4] != 0) {
        Actors_ActorPlacement(actor_id, &actor_location);
        yaw_delta = (instruction[3] << 4) - actor_location.location.position.pad;
        if (yaw_delta < -ONE / 2) {
            yaw_delta += ONE;
        }
        if (yaw_delta > ONE / 2) {
            yaw_delta -= ONE;
        }
        switch ((signed char)instruction[4]) {
        case -1:
            if (yaw_delta > 0) {
                yaw_delta -= ONE;
            }
            break;
        case 1:
            if (yaw_delta < 0) {
                yaw_delta += ONE;
            }
            break;
        }
        Actors_TurnActorByAtRate(actor_id, yaw_delta, turn_rate);
    } else {
        Actors_TurnActorToAtRate(actor_id, instruction[3] << 4, turn_rate);
    }
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7EF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1266)
intptr_t Event_ScriptFaceOtherActor(u_char* instruction, short thread_index)
{
    Actors_ActorLocationQuery target_location;
    int resolved_actor;
    int actor_id;
    int turn_rate;

    resolved_actor = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 3)), &target_location);
    turn_rate = -1;
    actor_id = resolved_actor & 0xFFFF;
    if (instruction[5] != 0xFF) {
        turn_rate = instruction[5];
    }
    Actors_TurnActorTowardTileAtRate(actor_id, &target_location.location.packedTile.target, turn_rate);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7F84 (src/BATTLE/BATTLE.PRG/4A0A8.c:1285)
intptr_t Event_ScriptFaceTile(u_char* instruction, short thread_index)
{
    Actors_TileTarget target_tile;
    int actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)) & 0xFFFF;

    target_tile.tileX = instruction[3];
    target_tile.tileLayer = instruction[4];
    target_tile.tileZ = instruction[5];

    Actors_TurnActorTowardTileAtRate(actor_id, &target_tile, instruction[6] != 0xFF ? instruction[6] : -1);
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B804C (src/BATTLE/BATTLE.PRG/4A0A8.c:1299)
intptr_t Event_ScriptScaleActor(u_char* instruction, short thread_index)
{
    SVECTOR scale_components;

    setVector(&scale_components, Core_Memory_ReadUnalignedShort(instruction + 3), Core_Memory_ReadUnalignedShort(instruction + 5),
        Core_Memory_ReadUnalignedShort(instruction + 7));
    Actors_ScaleActorToOverTime(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), &scale_components, instruction[9]);
    return 0;
}

// decomp: BATTLE.PRG func_800B80B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1309)
intptr_t Event_ScriptRemoveActorGroup(u_char* instruction, short thread_index)
{
    Event_ActorSelection actor_selection;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(instruction + 1), &actor_selection);
    while (1) {
        int actor_id = Event_NextScriptSelectedActor(&actor_selection);
        if (actor_id == 1) {
            break;
        }
        Actors_RemoveScriptedActor(actor_id);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B810C (src/BATTLE/BATTLE.PRG/4A0A8.c:1324)
intptr_t Event_ScriptWaitForActorFlags(u_char* instruction, short thread_index)
{
    short wait_mask;
    u_short wait_flags;
    int actor_id;
    u_short matched_wait_flags;

    actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)) & 0xFFFF;
    wait_flags = Actors_ActorStateFlags(actor_id) ^ 0x81;
    wait_mask = Core_Memory_ReadUnalignedShort(instruction + 3);

    if (wait_mask & 1) {
        if (Actors_IsScriptedActorPathFinished(actor_id) == 0) {
            wait_flags |= 1;
        }
    }
    matched_wait_flags = (wait_flags & wait_mask);
    return matched_wait_flags != 0;
}

// decomp: BATTLE.PRG func_800B81A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1344)
intptr_t Event_ScriptSetActorColorOverride(u_char* instruction, short thread_index)
{
    Render_ColorCode color_override;

    color_override.red = instruction[3];
    color_override.green = instruction[4];
    color_override.blue = instruction[5];

    if (color_override.red | color_override.green | color_override.blue) {
        Actors_SetActorTint(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), &color_override);
    } else {
        Actors_SetActorTint(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), 0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B822C (src/BATTLE/BATTLE.PRG/4A0A8.c:1360)
intptr_t Event_ScriptQueueActorAnimation(u_char* instruction, short thread_index)
{
    Actors_QueueActorAnimation(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)),
        Event_animationBankBases[instruction[5] >> 6] + instruction[3], instruction[5] & 0x3F, 0);
    return 0;
}

// decomp: BATTLE.PRG func_800B8290 (src/BATTLE/BATTLE.PRG/4A0A8.c:1367)
intptr_t Event_ScriptRotateActorAxes(u_char* instruction, short thread_index)
{
    SVECTOR rotation_components;
    u_short actor_id;
    short turn_rate;

    actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    turn_rate = -1;
    if (instruction[10] != 0xFF) {
        turn_rate = instruction[10];
    }
    rotation_components.vx = Core_Memory_ReadUnalignedShort(instruction + 4);
    rotation_components.vy = Core_Memory_ReadUnalignedShort(instruction + 6);
    rotation_components.vz = Core_Memory_ReadUnalignedShort(instruction + 8);
    if (instruction[3] != 0) {
        Actors_RotateActorByAtRate(actor_id, &rotation_components, turn_rate);
    } else {
        Actors_RotateActorToAtRate(actor_id, &rotation_components, turn_rate);
    }
    Event_scriptControlledActors[actor_id] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B8360 (src/BATTLE/BATTLE.PRG/4A0A8.c:1390)
intptr_t Event_ScriptSetActorHeadAngles(u_char* instruction, short thread_index)
{
    SVECTOR head_angles;
    int actor_id;
    int tracking_rate;

    head_angles.vx = Core_Memory_ReadUnalignedShort(instruction + 3);
    head_angles.vy = Core_Memory_ReadUnalignedShort(instruction + 5);
    head_angles.vz = 0;
    actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    tracking_rate = 0x1000;
    if (instruction[7] != 0) {
        tracking_rate = instruction[7];
    }
    Actors_SetActorHeadTracking(actor_id, -4, &head_angles, tracking_rate);
    return 0;
}

// decomp: BATTLE.PRG func_800B83D4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1408)
intptr_t Event_ScriptPlayActorAnimationAtFrame(u_char* instruction, short thread_index)
{
    Actors_PlayActorAnimationAtFrame(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)),
        Event_animationBankBases[instruction[6] >> 6] + instruction[3], instruction[6] & 0x3F, instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800B8438 (src/BATTLE/BATTLE.PRG/4A0A8.c:1415)
intptr_t Event_ScriptQueueActorAnimationAtFrame(u_char* instruction, short thread_index)
{
    Actors_QueueActorAnimation(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)),
        Event_animationBankBases[instruction[6] >> 6] + instruction[3], instruction[6] & 0x3F, instruction[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800B849C (src/BATTLE/BATTLE.PRG/4A0A8.c:1422)
intptr_t Event_ScriptSetActorIdleAnimation(u_char* instruction, short thread_index)
{
    int actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));

    if (instruction[4] == 0xFF) {
        Actors_SetActorTravelAnimation(actor_id, -1);
    } else {
        Actors_SetActorTravelAnimation(actor_id, Event_animationBankBases[instruction[3]] + instruction[4]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B8510 (src/BATTLE/BATTLE.PRG/4A0A8.c:1434)
intptr_t Event_ScriptLookAtActor(u_char* instruction, short thread_index)
{
    int tracking_rate;

    int resolved_target = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 3));
    int actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));

    int tracking_target = -2;
    if (resolved_target != 0x2000) {
        tracking_target = resolved_target;
    }

    tracking_rate = 0x1000;
    if (instruction[5] != 0) {
        tracking_rate = instruction[5];
    }
    Actors_SetActorHeadTracking(actor_id, tracking_target, 0, tracking_rate);
    return 0;
}

// decomp: BATTLE.PRG func_800B8590 (src/BATTLE/BATTLE.PRG/4A0A8.c:1454)
intptr_t Event_ScriptLookAtWorldPos(u_char* instruction, short thread_index)
{
    SVECTOR target_position;
    int actor_id;
    int tracking_rate;

    setVector(&target_position, Core_Memory_ReadUnalignedShort(instruction + 3), Core_Memory_ReadUnalignedShort(instruction + 5),
        Core_Memory_ReadUnalignedShort(instruction + 7));
    actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));
    tracking_rate = 0x1000;
    if (instruction[9] != 0) {
        tracking_rate = instruction[9];
    }
    Actors_SetActorHeadTracking(actor_id, -1, &target_position, tracking_rate);
    return 0;
}

// decomp: BATTLE.PRG func_800B860C (src/BATTLE/BATTLE.PRG/4A0A8.c:1471)
intptr_t Event_ScriptLoadObjectModel(u_char* instruction, short thread_index)
{
    switch ((instruction[3] >> 2) & 3) {
    case 0:
        Actors_QueueSharedSequenceLoad(instruction[3] & 3, Core_Memory_ReadUnalignedShort(instruction + 1));
        break;
    case 1:
        Actors_QueueSharedSequenceWithTextures(instruction[3] & 3, Core_Memory_ReadUnalignedShort(instruction + 1), (instruction[3] >> 4) + 0x1A);
        break;
    case 2:
        Actors_QueueSequenceEffectTextures(
            instruction[3] & 3, Core_Memory_ReadUnalignedShort(instruction + 1), (instruction[3] >> 4) + 0x1A);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B86E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1488)
intptr_t Event_ScriptWaitForObjectLoad(u_char* instruction, short thread_index) { return Actors_FirstFreeAssetLoadSlot() > 0; }

// decomp: BATTLE.PRG func_800B8708 (src/BATTLE/BATTLE.PRG/4A0A8.c:1490)
intptr_t Event_ScriptReleaseObjectModel(u_char* instruction, short thread_index)
{
    if (instruction[1] & 4) {
        Actors_ReleaseSequenceEffectTextures(instruction[1] & 3);
    } else {
        if (Actors_UnloadSharedSequence(instruction[1]) < 0) {
            return 1;
        }
    }
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800B8764 (src/BATTLE/BATTLE.PRG/4A0A8.c:1500)
intptr_t Event_ScriptSelectObjectTextureBank(u_char* instruction, short thread_index)
{
    Actors_SetSharedSequenceBank(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B8788 (src/BATTLE/BATTLE.PRG/4A0A8.c:1506)
intptr_t Event_ScriptSetActorOverlayPass(u_char* instruction, short thread_index)
{
    Render_ColorCode overlay_parameters[6];

    overlay_parameters[0].red = instruction[7];
    overlay_parameters[0].green = instruction[8];
    overlay_parameters[0].blue = instruction[9];

    if (overlay_parameters[0].red | overlay_parameters[0].green | overlay_parameters[0].blue) {
        overlay_parameters[2].red = instruction[3];
        overlay_parameters[2].green = instruction[4];
        overlay_parameters[2].blue = instruction[5];
        overlay_parameters[2].code = instruction[6];
        Actors_SetActorCompanion(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), 1, &overlay_parameters[2], &overlay_parameters[0]);
    } else {
        Actors_SetActorCompanion(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), 0, 0, 0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B884C (src/BATTLE/BATTLE.PRG/4A0A8.c:1526)
intptr_t Event_ScriptSetActorWeaponDrawn(u_char* instruction, short thread_index)
{
    Event_ActorSelection actor_selection;
    int actor_id;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(instruction + 1), &actor_selection);

    while (1) {
        actor_id = Event_NextScriptSelectedActor(&actor_selection);
        if (actor_id == 1) {
            break;
        }
        Battle_SetActorBattleMode(actor_id, Event_weaponDrawStates[instruction[3]]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B91F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1786)
intptr_t Event_ScriptPlayActorAnimationSound(u_char* instruction, short thread_index)
{
    Actors_PlayActorWeaponDrawSound(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9230 (src/BATTLE/BATTLE.PRG/4A0A8.c:1792)
intptr_t Event_ScriptSetNpcUpdatesEnabled(u_char* instruction, short thread_index)
{
    Actors_SetOtherActorLogicEnabled(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B92A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1807)
intptr_t Event_ScriptRestoreActorVitals(u_char* instruction, short thread_index)
{
    Actors_RestoreActorVitals(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)));
    return 0;
}

// decomp: BATTLE.PRG func_800B9608 (src/BATTLE/BATTLE.PRG/4A0A8.c:1916)
intptr_t Event_ScriptSetActorEquipmentModels(u_char* instruction, short thread_index)
{
    u_short actor_id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1));

    if (instruction[3] == 0xFE) {
        Actors_RefreshActorEquipmentModels(actor_id);
    } else {
        Actors_LoadActorEquipmentModels(actor_id, instruction[3], instruction[5]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B9670 (src/BATTLE/BATTLE.PRG/4A0A8.c:1928)
intptr_t Event_ScriptSetActorYawOscillation(u_char* instruction, short thread_index)
{
    Actors_SetActorPaletteFade(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B97F8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1988)
intptr_t Event_ScriptClearAshleyStatusEffects(u_char* instruction, short thread_index)
{
    Battle_PreserveAshleyStatuses();
    return 0;
}

// decomp: BATTLE.PRG func_800B9D74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2151)
intptr_t Event_ScriptSetActorRenderMode(u_char* instruction, short thread_index)
{
    Actors_StartActorAppearanceTransition(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), instruction[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAC38 (src/BATTLE/BATTLE.PRG/4A0A8.c:2549)
intptr_t Event_CommandSetEnteredDoor(u_char* instruction, short thread_index)
{
    Field_MarkEntranceDoorOnSceneMap(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAC5C (src/BATTLE/BATTLE.PRG/4A0A8.c:2555)
intptr_t Event_CommandStoreActorRangeBearing(u_char* instruction, short thread_index)
{
    Actors_ActorLocationQuery first_actor_location;
    Actors_ActorLocationQuery second_actor_location;
    short result_flag_id;
    short delta_x;
    short delta_z;
    int scaled_distance;

    result_flag_id = Core_Memory_ReadUnalignedShort(instruction + 6);
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), &first_actor_location);
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 3)), &second_actor_location);
    delta_x = first_actor_location.location.position.vx - second_actor_location.location.position.vx;
    delta_z = first_actor_location.location.position.vz - second_actor_location.location.position.vz;
    scaled_distance = Core_Math_SquareRootInteger((delta_x * delta_x) + (delta_z * delta_z)) / instruction[5];
    State_SetScriptFlag(result_flag_id, scaled_distance < 0x100 ? scaled_distance & 0xFF : -1);
    State_SetScriptFlag(result_flag_id + 1, (Core_Gte_Atan2(delta_x, delta_z) >> 4) & 0xFF);
    return 0;
}

// decomp: BATTLE.PRG func_800BAD78 (src/BATTLE/BATTLE.PRG/4A0A8.c:2575)
intptr_t Event_CommandSetActorActivity(u_char* instruction, short thread_index)
{
    Event_ActorSelection actor_selection;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(instruction + 1), &actor_selection);
    while (1) {
        int actor_id = Event_NextScriptSelectedActor(&actor_selection);
        if (actor_id == 1) {
            break;
        }
        Actors_SetActorLogicEnabled(actor_id, instruction[3]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BADE0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2590)
intptr_t Event_CommandAwardMiscLoot(u_char* instruction, short thread_index)
{
    if (Event_isMenuTransitionActive == 0) {
        Items_PresentScriptedLoot(
            Core_Memory_ReadUnalignedShort(instruction + 1), instruction[3], Core_Memory_ReadUnalignedShort(instruction + 4), instruction[6]);
        Event_isMenuTransitionActive = 1;
        return 1;
    }

    if (Battle_UpdateRewardSequence() == 0) {
        Event_isMenuTransitionActive = 0;
        return 4;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800BAE74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2606)
intptr_t Event_CommandConsumeMiscItem(u_char* instruction, short thread_index)
{
    Items_ConsumeMiscItem(Core_Memory_ReadUnalignedShort(instruction + 1));
    return 0;
}

// decomp: BATTLE.PRG func_800BAEA0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2612)
intptr_t Event_CommandStoreActorDistance(u_char* instruction, short thread_index)
{
    Actors_ActorLocationQuery first_actor_location;
    Actors_ActorLocationQuery second_actor_location;
    short delta_x;
    int distance_flag_id;
    short delta_z;
    int horizontal_distance;

    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)), &first_actor_location);
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 3)), &second_actor_location);
    delta_x = first_actor_location.location.position.vx - second_actor_location.location.position.vx;
    delta_z = first_actor_location.location.position.vz - second_actor_location.location.position.vz;
    distance_flag_id = Core_Memory_ReadUnalignedShort(instruction + 5);
    horizontal_distance = Core_Math_SquareRootInteger((delta_x * delta_x) + (delta_z * delta_z));
    if (horizontal_distance < 0) {
        horizontal_distance += 0x7F;
    }
    State_SetScriptFlag(distance_flag_id, (horizontal_distance >> 7) & 0xFF);
    return 0;
}

// decomp: BATTLE.PRG func_800BAF6C (src/BATTLE/BATTLE.PRG/4A0A8.c:2634)
void Event_SetEventActorActivity(short activity_enabled)
{
    short actorId;

    for (actorId = 1; actorId < 17; ++actorId) {
        if (Actors_ActorAtSlot(actorId) != NULL) {
            if (activity_enabled != 0) {
                Actors_SetActorDisplayFlags(actorId, Actors_ActorStateFlags(actorId) | 1);
            } else {
                Actors_SetActorDisplayFlags(actorId, Actors_ActorStateFlags(actorId) & ~1);
            }
        }
    }

    Actors_SetOtherActorLogicEnabled(activity_enabled);
}

// decomp: BATTLE.PRG func_800BB68C (src/BATTLE/BATTLE.PRG/4A0A8.c:2880)
void Event_BuildScriptActorSelection(u_short actor_selector, Event_ActorSelection* selection)
{
    Battle_Actor* actor;
    short write_index = 0;
    selection->remainingCount = 0;
    selection->cursor = 0;

    if (actor_selector & 0x2000) {
        short i;
        short kind_filter = -1;
        if (actor_selector == 0x2003) {
            kind_filter = 4;
        }

        for (i = 0; i < 17; ++i) {
            actor = Actors_ActorAtSlot(i);
            if (actor != NULL) {
                if ((kind_filter < 0) || (actor->kind == kind_filter)) {
                    selection->actorIds[write_index++] = i;
                    ++selection->remainingCount;
                }
            }
        }

        return;
    }
    ++selection->remainingCount;
    selection->actorIds[0] = Event_ResolveScriptActorReference(actor_selector);
}

// decomp: BATTLE.PRG func_800BB788 (src/BATTLE/BATTLE.PRG/4A0A8.c:2910)
u_short Event_NextScriptSelectedActor(Event_ActorSelection* selection)
{
    if (selection->remainingCount == 0) {
        return 1;
    }

    --selection->remainingCount;
    return selection->actorIds[(u_int)(selection->cursor++)];
}

// decomp: BATTLE.PRG func_800BB7C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2920)
int Event_ActorDialogAnchor(int actor_id, SVECTOR* dialog_anchor)
{
    MATRIX tagged_bone_matrix;
    long anchor_projection_workspace[5];

    dialog_anchor->vx = 0;
    dialog_anchor->vy = -0x3E8;
    dialog_anchor->vz = 0;
    Actors_ActorTaggedBoneMatrix(actor_id, 1, &tagged_bone_matrix, (u_long *)(&anchor_projection_workspace[2]));
    Core_Gte_SetRotMatrix(&tagged_bone_matrix);
    Core_Gte_SetTransMatrix(&tagged_bone_matrix);
    Core_Gte_RotTransPers(dialog_anchor, anchor_projection_workspace, &anchor_projection_workspace[3], &anchor_projection_workspace[4]);
    Actors_ProjectActorTaggedBone(actor_id, 1, dialog_anchor, anchor_projection_workspace[2]);
    dialog_anchor->vz = Core_Gte_Atan2(((short*)&anchor_projection_workspace)[1] - dialog_anchor->vy, (short)anchor_projection_workspace[0] - dialog_anchor->vx);
    return dialog_anchor->vz;
}

}  // extern "C"
