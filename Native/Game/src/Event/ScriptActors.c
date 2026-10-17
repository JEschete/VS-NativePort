// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorLoading.h"
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
#include "Core/Math/FixedPoint.h"
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
int Event_ScriptResetActorGroupState(u_char* arg0, short arg1)
{
    Event_ActorSelection sp10;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(arg0 + 1), &sp10);
    while (1) {
        int temp_a0 = Event_NextScriptSelectedActor(&sp10);
        if (temp_a0 == 1) {
            break;
        }
        Actors_InterruptActorAction(temp_a0, arg0[3]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B7490 (src/BATTLE/BATTLE.PRG/4A0A8.c:983)
int Event_ScriptStoreActorTile(u_char* arg0, short arg1)
{
    u_char position[12];
    u_short actor = Core_Memory_ReadUnalignedShort(arg0 + 1);
    u_short flag = Core_Memory_ReadUnalignedShort(arg0 + 3);

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
int Event_ScriptSpawnActor(u_char* arg0, short arg1)
{
    u_char sp10[12];

    sp10[0] = arg0[5];
    sp10[1] = arg0[6];
    sp10[2] = arg0[7];
    sp10[3] = arg0[8];
    Actors_QueueScriptedActorSpawn(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)),
        Core_Memory_ReadUnalignedShort(arg0 + 3), (Field_ActorPlacement*)(sp10));
    return 0;
}

// decomp: BATTLE.PRG func_800B75F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1022)
int Event_ScriptNavigateActorToTile(u_char* arg0, short arg1)
{
    SVECTOR sp10;
    u_short temp_v0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    sp10.vx = (arg0[3] << 7) + 0x40;
    sp10.vy = 0;
    sp10.vz = (arg0[5] << 7) + 0x40;
    if (Event_skipState == 2) {
        sp10.pad = -0x8000;
        Actors_TranslateActorToAtRate(temp_v0, &sp10, 0);
    } else {
        Actors_BeginScriptedActorPath(temp_v0, &sp10);
    }
    Actors_ClearActorMovementModes(temp_v0);
    Event_scriptControlledActors[temp_v0] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B76A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1040)
int Event_ScriptPlayActorAnimation(u_char* arg0)
{
    Actors_PlayActorAnimation(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)),
        Event_animationBankBases[arg0[5] >> 6] + arg0[3], arg0[4], arg0[5] & 0x3F);
    return 0;
}

// decomp: BATTLE.PRG func_800B770C (src/BATTLE/BATTLE.PRG/4A0A8.c:1047)
int Event_ScriptSetActorGroupFlags(u_char* arg0, short arg1)
{
    Event_ActorSelection sp10;
    int temp_s0;
    int var_s1;
    int var_s2;
    int temp_v0;
    u_int a1;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(arg0 + 1), &sp10);

    while (1) {
        temp_s0 = Event_NextScriptSelectedActor(&sp10);
        if (temp_s0 == 1) {
            break;
        }
        temp_v0 = Actors_ActorStateFlags(temp_s0);
        var_s2 = temp_v0 & 0x800;
        var_s1 = temp_v0 & 0x1000;
        a1 = arg0[4];
        if (!(a1 & 2)) {
            var_s2 = (a1 & 1) << 0xB;
        }
        if (!(a1 & 8)) {
            var_s1 = (a1 << 0xA) & 0x1000;
        }
        Actors_SetActorDetached(temp_s0, (a1 >> 5) & 1);
        Actors_SetActorDisplayFlags(temp_s0, var_s2 | var_s1 | arg0[3]);
        Actors_ResetActorWeaponVisibilityBlend(temp_s0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B77DC (src/BATTLE/BATTLE.PRG/4A0A8.c:1080)
int Event_ScriptSetActorSoundMix(u_char* arg0, short arg1)
{
    Actors_ModelEffectSlot sp10;
    int var_a0;

    sp10.kind = arg0[3];
    sp10.level = arg0[4];
    sp10.flags = arg0[5];
    sp10.parameter = arg0[6];
    var_a0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    if (var_a0 == 0x2001) {
        var_a0 = -1;
    }
    Actors_SetActorSoundOverrides(var_a0, &sp10);
    return 0;
}

// decomp: BATTLE.PRG func_800B784C (src/BATTLE/BATTLE.PRG/4A0A8.c:1097)
int Event_ScriptWalkActorToTile(u_char* arg0, short arg1)
{
    u_char sp10[10];
    u_short temp_v0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    sp10[0] = arg0[3];
    sp10[1] = arg0[4];
    sp10[2] = arg0[5];
    sp10[3] = arg0[6];
    if (Event_skipState == 2) {
        Actors_MoveActorToTileAtRate(temp_v0, (Actors_TileTarget*)sp10, 0, 0);
    } else {
        Actors_MoveActorToTileAtRate(temp_v0, (Actors_TileTarget*)sp10,
            arg0[7] != 0xFF ? arg0[7] : -1, arg0[8] != 0xFF ? arg0[8] : -1);
    }
    Actors_ClearActorMovementModes(temp_v0);
    Event_scriptControlledActors[temp_v0] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B792C (src/BATTLE/BATTLE.PRG/4A0A8.c:1116)
int Event_ScriptWalkActorToWorldPos(u_char* arg0, short arg1)
{
    Actors_ActorLocationQuery sp10;

    u_short temp_s4 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    int temp_s1 = Core_Memory_ReadUnalignedShort(arg0 + 3) >> 1;
    short temp_s2 = Core_Memory_ReadUnalignedShort(arg0 + 5);
    short temp_s3 = Core_Memory_ReadUnalignedShort(arg0 + 7);

    if (arg0[3] & 1) {
        Actors_ActorPlacement(temp_s4, &sp10);
        applyVector(&sp10.location.position, temp_s1, temp_s2, temp_s3, +=);
    } else {
        setVector(&sp10.location.position, temp_s1, temp_s2, temp_s3);
    }
    if (arg0[9] != 0xFF) {
        char v = 0x10;
        sp10.location.position.pad = arg0[9] * v;
    } else {
        sp10.location.position.pad = -0x8000;
    }
    if (Event_skipState == 2) {
        Actors_MoveActorToPositionAtRate(temp_s4, &sp10.location.position, 0, 0);
    } else {
        Actors_MoveActorToPositionAtRate(temp_s4, &sp10.location.position, arg0[10] != 0xFF ? arg0[10] : -1,
            arg0[11] != 0xFF ? arg0[11] : -1);
    }
    Actors_ClearActorMovementModes(temp_s4);
    Event_scriptControlledActors[temp_s4] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7A98 (src/BATTLE/BATTLE.PRG/4A0A8.c:1148)
int Event_ScriptSlideActorToTile(u_char* arg0, short arg1)
{
    u_char sp10[16];
    u_short temp_s0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    sp10[0] = arg0[3];
    sp10[1] = arg0[4];
    sp10[2] = arg0[5];
    sp10[3] = 0xFF;
    if (Event_skipState == 2) {
        Actors_TranslateActorToTileAtRate(temp_s0, (Actors_TileTarget*)sp10, 0);
    } else {
        Actors_TranslateActorToTileAtRate(
            temp_s0, (Actors_TileTarget*)sp10, arg0[6] != 0xFF ? arg0[6] : -1);
    }
    Actors_ClearActorMovementModes(temp_s0);
    Event_scriptControlledActors[temp_s0] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7B58 (src/BATTLE/BATTLE.PRG/4A0A8.c:1167)
int Event_ScriptSlideActorToWorldPos(u_char* arg0, short arg1)
{
    Actors_ActorLocationQuery sp10;
    short temp_s3;
    short temp_s4;
    int temp_s1;
    u_short temp_s2;

    temp_s2 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    temp_s1 = Core_Memory_ReadUnalignedShort(arg0 + 3) >> 1;
    temp_s3 = Core_Memory_ReadUnalignedShort(arg0 + 5);
    temp_s4 = Core_Memory_ReadUnalignedShort(arg0 + 7);

    if (arg0[3] & 1) {
        Actors_ActorPlacement(temp_s2, &sp10);
        applyVector(&sp10.location.position, temp_s1, temp_s3, temp_s4, +=);
    } else {
        setVector(&sp10.location.position, temp_s1, temp_s3, temp_s4);
    }

    sp10.location.position.pad = -0x8000;

    if (Event_skipState == 2) {
        Actors_TranslateActorToAtRate(temp_s2, &sp10.location.position, 0);
    } else {
        Actors_TranslateActorToAtRate(temp_s2, &sp10.location.position, arg0[9] != 0xFF ? arg0[9] : -1);
    }

    Actors_ClearActorMovementModes(temp_s2);
    Event_scriptControlledActors[temp_s2] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7C90 (src/BATTLE/BATTLE.PRG/4A0A8.c:1200)
int Event_ScriptSetActorAttachment(u_char* arg0, short arg1)
{
    u_short temp_s1 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    u_int temp_a1 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)) & 0xFFFF;
    if (temp_a1 >= 0x2000) {
        Actors_ReleaseActorAttachmentLock(temp_s1, temp_a1);
    } else {
        Actors_AttachActorToParentBone(temp_s1, temp_a1, arg0[5], arg0[6]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B7D10 (src/BATTLE/BATTLE.PRG/4A0A8.c:1212)
int Event_ScriptNavigateActorToWorldPos(u_char* arg0, short arg1)
{
    SVECTOR sp10;
    u_short temp_s1 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1) & 0xFFFF);
    sp10.vx = Core_Memory_ReadUnalignedShort(arg0 + 3);
    sp10.vy = 0;
    sp10.vz = Core_Memory_ReadUnalignedShort(arg0 + 5);
    if (Event_skipState == 2) {
        sp10.pad = -0x8000;
        Actors_TranslateActorToAtRate(temp_s1, &sp10, 0);
    } else {
        Actors_BeginScriptedActorPath(temp_s1, &sp10);
    }
    Actors_ClearActorMovementModes(temp_s1);
    Event_scriptControlledActors[temp_s1] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7DC4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1230)
int Event_ScriptTurnActorToFacing(u_char* arg0, short arg1)
{
    Actors_ActorLocationQuery sp10;
    u_short temp_s1 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    short var_s2 = arg0[5] != 0xFF ? arg0[5] : -1;
    int var_a1;

    if (arg0[4] != 0) {
        Actors_ActorPlacement(temp_s1, &sp10);
        var_a1 = (arg0[3] << 4) - sp10.location.position.pad;
        if (var_a1 < -ONE / 2) {
            var_a1 += ONE;
        }
        if (var_a1 > ONE / 2) {
            var_a1 -= ONE;
        }
        switch ((signed char)arg0[4]) {
        case -1:
            if (var_a1 > 0) {
                var_a1 -= ONE;
            }
            break;
        case 1:
            if (var_a1 < 0) {
                var_a1 += ONE;
            }
            break;
        }
        Actors_TurnActorByAtRate(temp_s1, var_a1, var_s2);
    } else {
        Actors_TurnActorToAtRate(temp_s1, arg0[3] << 4, var_s2);
    }
    Event_scriptControlledActors[temp_s1] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7EF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1266)
int Event_ScriptFaceOtherActor(u_char* arg0, short arg1)
{
    Actors_ActorLocationQuery sp10;
    int temp_s0;
    int temp_s0_2;
    int var_a2;

    temp_s0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)), &sp10);
    var_a2 = -1;
    temp_s0_2 = temp_s0 & 0xFFFF;
    if (arg0[5] != 0xFF) {
        var_a2 = arg0[5];
    }
    Actors_TurnActorTowardTileAtRate(temp_s0_2, &sp10.location.packedTile.target, var_a2);
    Event_scriptControlledActors[temp_s0_2] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B7F84 (src/BATTLE/BATTLE.PRG/4A0A8.c:1285)
int Event_ScriptFaceTile(u_char* arg0, short arg1)
{
    Actors_TileTarget sp10;
    int temp_s0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)) & 0xFFFF;

    sp10.tileX = arg0[3];
    sp10.tileLayer = arg0[4];
    sp10.tileZ = arg0[5];

    Actors_TurnActorTowardTileAtRate(temp_s0, &sp10, arg0[6] != 0xFF ? arg0[6] : -1);
    Event_scriptControlledActors[temp_s0] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B804C (src/BATTLE/BATTLE.PRG/4A0A8.c:1299)
int Event_ScriptScaleActor(u_char* arg0, short arg1)
{
    SVECTOR sp10;

    setVector(&sp10, Core_Memory_ReadUnalignedShort(arg0 + 3), Core_Memory_ReadUnalignedShort(arg0 + 5),
        Core_Memory_ReadUnalignedShort(arg0 + 7));
    Actors_ScaleActorToOverTime(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), &sp10, arg0[9]);
    return 0;
}

// decomp: BATTLE.PRG func_800B80B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1309)
int Event_ScriptRemoveActorGroup(u_char* arg0, short arg1)
{
    Event_ActorSelection sp10;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(arg0 + 1), &sp10);
    while (1) {
        int temp_a0 = Event_NextScriptSelectedActor(&sp10);
        if (temp_a0 == 1) {
            break;
        }
        Actors_RemoveScriptedActor(temp_a0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B810C (src/BATTLE/BATTLE.PRG/4A0A8.c:1324)
int Event_ScriptWaitForActorFlags(u_char* arg0)
{
    short temp_v0;
    u_short temp_s3;
    int temp_s2;
    u_short var_v0;

    temp_s2 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)) & 0xFFFF;
    temp_s3 = Actors_ActorStateFlags(temp_s2) ^ 0x81;
    temp_v0 = Core_Memory_ReadUnalignedShort(arg0 + 3);

    if (temp_v0 & 1) {
        if (Actors_IsScriptedActorPathFinished(temp_s2) == 0) {
            temp_s3 |= 1;
        }
    }
    var_v0 = (temp_s3 & temp_v0);
    return var_v0 != 0;
}

// decomp: BATTLE.PRG func_800B81A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1344)
int Event_ScriptSetActorColorOverride(u_char* arg0, short arg1)
{
    Render_ColorCode sp10;

    sp10.red = arg0[3];
    sp10.green = arg0[4];
    sp10.blue = arg0[5];

    if (sp10.red | sp10.green | sp10.blue) {
        Actors_SetActorTint(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), &sp10);
    } else {
        Actors_SetActorTint(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), 0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B822C (src/BATTLE/BATTLE.PRG/4A0A8.c:1360)
int Event_ScriptQueueActorAnimation(u_char* arg0)
{
    Actors_QueueActorAnimation(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)),
        Event_animationBankBases[arg0[5] >> 6] + arg0[3], arg0[5] & 0x3F, 0);
    return 0;
}

// decomp: BATTLE.PRG func_800B8290 (src/BATTLE/BATTLE.PRG/4A0A8.c:1367)
int Event_ScriptRotateActorAxes(u_char* arg0, short arg1)
{
    SVECTOR sp10;
    u_short temp_s2;
    short var_s1;

    temp_s2 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    var_s1 = -1;
    if (arg0[10] != 0xFF) {
        var_s1 = arg0[10];
    }
    sp10.vx = Core_Memory_ReadUnalignedShort(arg0 + 4);
    sp10.vy = Core_Memory_ReadUnalignedShort(arg0 + 6);
    sp10.vz = Core_Memory_ReadUnalignedShort(arg0 + 8);
    if (arg0[3] != 0) {
        Actors_RotateActorByAtRate(temp_s2, &sp10, var_s1);
    } else {
        Actors_RotateActorToAtRate(temp_s2, &sp10, var_s1);
    }
    Event_scriptControlledActors[temp_s2] = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800B8360 (src/BATTLE/BATTLE.PRG/4A0A8.c:1390)
int Event_ScriptSetActorHeadAngles(u_char* arg0, short arg1)
{
    SVECTOR sp10;
    int temp_v0;
    int var_a3;

    sp10.vx = Core_Memory_ReadUnalignedShort(arg0 + 3);
    sp10.vy = Core_Memory_ReadUnalignedShort(arg0 + 5);
    sp10.vz = 0;
    temp_v0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    var_a3 = 0x1000;
    if (arg0[7] != 0) {
        var_a3 = arg0[7];
    }
    Actors_SetActorHeadTracking(temp_v0, -4, &sp10, var_a3);
    return 0;
}

// decomp: BATTLE.PRG func_800B83D4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1408)
int Event_ScriptPlayActorAnimationAtFrame(u_char* arg0)
{
    Actors_PlayActorAnimationAtFrame(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)),
        Event_animationBankBases[arg0[6] >> 6] + arg0[3], arg0[6] & 0x3F, arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800B8438 (src/BATTLE/BATTLE.PRG/4A0A8.c:1415)
int Event_ScriptQueueActorAnimationAtFrame(u_char* arg0)
{
    Actors_QueueActorAnimation(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)),
        Event_animationBankBases[arg0[6] >> 6] + arg0[3], arg0[6] & 0x3F, arg0[4]);
    return 0;
}

// decomp: BATTLE.PRG func_800B849C (src/BATTLE/BATTLE.PRG/4A0A8.c:1422)
int Event_ScriptSetActorIdleAnimation(u_char* arg0, short arg1)
{
    int temp_a0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));

    if (arg0[4] == 0xFF) {
        Actors_SetActorTravelAnimation(temp_a0, -1);
    } else {
        Actors_SetActorTravelAnimation(temp_a0, Event_animationBankBases[arg0[3]] + arg0[4]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B8510 (src/BATTLE/BATTLE.PRG/4A0A8.c:1434)
int Event_ScriptLookAtActor(u_char* arg0, short arg1)
{
    int var_a3;

    int temp_s0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3));
    int temp_a0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));

    int var_a1 = -2;
    if (temp_s0 != 0x2000) {
        var_a1 = temp_s0;
    }

    var_a3 = 0x1000;
    if (arg0[5] != 0) {
        var_a3 = arg0[5];
    }
    Actors_SetActorHeadTracking(temp_a0, var_a1, 0, var_a3);
    return 0;
}

// decomp: BATTLE.PRG func_800B8590 (src/BATTLE/BATTLE.PRG/4A0A8.c:1454)
int Event_ScriptLookAtWorldPos(u_char* arg0, short arg1)
{
    SVECTOR sp10;
    int temp_v0;
    int var_a3;

    setVector(&sp10, Core_Memory_ReadUnalignedShort(arg0 + 3), Core_Memory_ReadUnalignedShort(arg0 + 5),
        Core_Memory_ReadUnalignedShort(arg0 + 7));
    temp_v0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));
    var_a3 = 0x1000;
    if (arg0[9] != 0) {
        var_a3 = arg0[9];
    }
    Actors_SetActorHeadTracking(temp_v0, -1, &sp10, var_a3);
    return 0;
}

// decomp: BATTLE.PRG func_800B860C (src/BATTLE/BATTLE.PRG/4A0A8.c:1471)
int Event_ScriptLoadObjectModel(u_char* arg0, short arg1)
{
    switch ((arg0[3] >> 2) & 3) {
    case 0:
        Actors_QueueSharedSequenceLoad(arg0[3] & 3, Core_Memory_ReadUnalignedShort(arg0 + 1));
        break;
    case 1:
        Actors_QueueSharedSequenceWithTextures(arg0[3] & 3, Core_Memory_ReadUnalignedShort(arg0 + 1), (arg0[3] >> 4) + 0x1A);
        break;
    case 2:
        Actors_QueueSequenceEffectTextures(
            arg0[3] & 3, Core_Memory_ReadUnalignedShort(arg0 + 1), (arg0[3] >> 4) + 0x1A);
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B86E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1488)
int Event_ScriptWaitForObjectLoad(u_char* arg0, short arg1) { return Actors_FirstFreeAssetLoadSlot() > 0; }

// decomp: BATTLE.PRG func_800B8708 (src/BATTLE/BATTLE.PRG/4A0A8.c:1490)
int Event_ScriptReleaseObjectModel(u_char* arg0, short arg1)
{
    if (arg0[1] & 4) {
        Actors_ReleaseSequenceEffectTextures(arg0[1] & 3);
    } else {
        Actors_UnloadSharedSequence(arg0[1]);
    }
    return (Event_skipState == 2) * 4;
}

// decomp: BATTLE.PRG func_800B8764 (src/BATTLE/BATTLE.PRG/4A0A8.c:1500)
int Event_ScriptSelectObjectTextureBank(u_char* arg0, short arg1)
{
    Actors_SetSharedSequenceBank(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B8788 (src/BATTLE/BATTLE.PRG/4A0A8.c:1506)
int Event_ScriptSetActorOverlayPass(u_char* arg0, short arg1)
{
    Render_ColorCode sp10[6];

    sp10[0].red = arg0[7];
    sp10[0].green = arg0[8];
    sp10[0].blue = arg0[9];

    if (sp10[0].red | sp10[0].green | sp10[0].blue) {
        sp10[2].red = arg0[3];
        sp10[2].green = arg0[4];
        sp10[2].blue = arg0[5];
        sp10[2].code = arg0[6];
        Actors_SetActorCompanion(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), 1, &sp10[2], &sp10[0]);
    } else {
        Actors_SetActorCompanion(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), 0, 0, 0);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B884C (src/BATTLE/BATTLE.PRG/4A0A8.c:1526)
int Event_ScriptSetActorWeaponDrawn(u_char* arg0, short arg1)
{
    Event_ActorSelection sp10;
    int temp_a0;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(arg0 + 1), &sp10);

    while (1) {
        temp_a0 = Event_NextScriptSelectedActor(&sp10);
        if (temp_a0 == 1) {
            break;
        }
        Battle_SetActorBattleMode(temp_a0, Event_weaponDrawStates[arg0[3]]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B91F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1786)
int Event_ScriptPlayActorAnimationSound(u_char* arg0, short arg1)
{
    Actors_PlayActorWeaponDrawSound(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), arg0[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9230 (src/BATTLE/BATTLE.PRG/4A0A8.c:1792)
int Event_ScriptSetNpcUpdatesEnabled(u_char* arg0, short arg1)
{
    Actors_SetOtherActorLogicEnabled(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B92A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1807)
int Event_ScriptRestoreActorVitals(u_char* arg0, short arg1)
{
    Actors_RestoreActorVitals(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)));
    return 0;
}

// decomp: BATTLE.PRG func_800B9608 (src/BATTLE/BATTLE.PRG/4A0A8.c:1916)
int Event_ScriptSetActorEquipmentModels(u_char* arg0, short arg1)
{
    u_short temp_a0 = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1));

    if (arg0[3] == 0xFE) {
        Actors_RefreshActorEquipmentModels(temp_a0);
    } else {
        Actors_LoadActorEquipmentModels(temp_a0, arg0[3], arg0[5]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800B9670 (src/BATTLE/BATTLE.PRG/4A0A8.c:1928)
int Event_ScriptSetActorYawOscillation(u_char* arg0, short arg1)
{
    Actors_SetActorPaletteFade(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), arg0[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B97F8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1988)
int Event_ScriptClearAshleyStatusEffects(u_char* arg0, short arg1)
{
    Battle_PreserveAshleyStatuses();
    return 0;
}

// decomp: BATTLE.PRG func_800B9D74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2151)
int Event_ScriptSetActorRenderMode(u_char* arg0, short arg1)
{
    Actors_StartActorAppearanceTransition(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), arg0[3]);
    return 0;
}

// decomp: BATTLE.PRG func_800B9DB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2158) [INCLUDE_ASM: not yet ported]
int Event_ScriptSetAshleyBattleMode(u_char*, short)
{
    VS_UNPORTED("Event::ScriptSetAshleyBattleMode");
    return 0;
}

// decomp: BATTLE.PRG func_800BAC38 (src/BATTLE/BATTLE.PRG/4A0A8.c:2549)
int Event_CommandSetEnteredDoor(u_char* arg0, short arg1)
{
    Field_MarkEntranceDoorOnSceneMap(arg0[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BAC5C (src/BATTLE/BATTLE.PRG/4A0A8.c:2555)
int Event_CommandStoreActorRangeBearing(u_char* arg0, short arg1)
{
    Actors_ActorLocationQuery sp10;
    Actors_ActorLocationQuery sp20;
    short temp_s3;
    short temp_v0;
    short temp_v0_2;
    int temp_lo;

    temp_s3 = Core_Memory_ReadUnalignedShort(arg0 + 6);
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), &sp10);
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)), &sp20);
    temp_v0 = sp10.location.position.vx - sp20.location.position.vx;
    temp_v0_2 = sp10.location.position.vz - sp20.location.position.vz;
    temp_lo = Core_Math_SquareRootInteger((temp_v0 * temp_v0) + (temp_v0_2 * temp_v0_2)) / arg0[5];
    State_SetScriptFlag(temp_s3, temp_lo < 0x100 ? temp_lo & 0xFF : -1);
    State_SetScriptFlag(temp_s3 + 1, (Core_Gte_Atan2(temp_v0, temp_v0_2) >> 4) & 0xFF);
    return 0;
}

// decomp: BATTLE.PRG func_800BAD78 (src/BATTLE/BATTLE.PRG/4A0A8.c:2575)
int Event_CommandSetActorActivity(u_char* arg0, short arg1)
{
    Event_ActorSelection sp10;

    Event_BuildScriptActorSelection(Core_Memory_ReadUnalignedShort(arg0 + 1), &sp10);
    while (1) {
        int temp_a0 = Event_NextScriptSelectedActor(&sp10);
        if (temp_a0 == 1) {
            break;
        }
        Actors_SetActorLogicEnabled(temp_a0, arg0[3]);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BADE0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2590)
int Event_CommandAwardMiscLoot(u_char* arg0)
{
    if (Event_isMenuTransitionActive == 0) {
        Items_PresentScriptedLoot(
            Core_Memory_ReadUnalignedShort(arg0 + 1), arg0[3], Core_Memory_ReadUnalignedShort(arg0 + 4), arg0[6]);
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
int Event_CommandConsumeMiscItem(u_char* arg0, short arg1)
{
    Items_ConsumeMiscItem(Core_Memory_ReadUnalignedShort(arg0 + 1));
    return 0;
}

// decomp: BATTLE.PRG func_800BAEA0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2612)
int Event_CommandStoreActorDistance(u_char* arg0, short arg1)
{
    Actors_ActorLocationQuery sp10;
    Actors_ActorLocationQuery sp20;
    short temp_s0;
    int temp_s0_2;
    short temp_s1;
    int var_v0;

    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 1)), &sp10);
    Actors_ActorPlacement(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(arg0 + 3)), &sp20);
    temp_s0 = sp10.location.position.vx - sp20.location.position.vx;
    temp_s1 = sp10.location.position.vz - sp20.location.position.vz;
    temp_s0_2 = Core_Memory_ReadUnalignedShort(arg0 + 5);
    var_v0 = Core_Math_SquareRootInteger((temp_s0 * temp_s0) + (temp_s1 * temp_s1));
    if (var_v0 < 0) {
        var_v0 += 0x7F;
    }
    State_SetScriptFlag(temp_s0_2, (var_v0 >> 7) & 0xFF);
    return 0;
}

// decomp: BATTLE.PRG func_800BAF6C (src/BATTLE/BATTLE.PRG/4A0A8.c:2634)
void Event_SetEventActorActivity(short arg0)
{
    short actorId;

    for (actorId = 1; actorId < 17; ++actorId) {
        if (Actors_ActorAtSlot(actorId) != NULL) {
            if (arg0 != 0) {
                Actors_SetActorDisplayFlags(actorId, Actors_ActorStateFlags(actorId) | 1);
            } else {
                Actors_SetActorDisplayFlags(actorId, Actors_ActorStateFlags(actorId) & ~1);
            }
        }
    }

    Actors_SetOtherActorLogicEnabled(arg0);
}

// decomp: BATTLE.PRG func_800BB68C (src/BATTLE/BATTLE.PRG/4A0A8.c:2880)
void Event_BuildScriptActorSelection(u_short arg0, Event_ActorSelection* arg1)
{
    Battle_Actor* temp_v0;
    short var_s3 = 0;
    arg1->remainingCount = 0;
    arg1->cursor = 0;

    if (arg0 & 0x2000) {
        short i;
        short var_a0 = -1;
        if (arg0 == 0x2003) {
            var_a0 = 4;
        }

        for (i = 0; i < 17; ++i) {
            temp_v0 = Actors_ActorAtSlot(i);
            if (temp_v0 != NULL) {
                if ((var_a0 < 0) || (temp_v0->kind == var_a0)) {
                    arg1->actorIds[var_s3++] = i;
                    ++arg1->remainingCount;
                }
            }
        }

        return;
    }
    ++arg1->remainingCount;
    arg1->actorIds[0] = Event_ResolveScriptActorReference(arg0);
}

// decomp: BATTLE.PRG func_800BB788 (src/BATTLE/BATTLE.PRG/4A0A8.c:2910)
u_short Event_NextScriptSelectedActor(Event_ActorSelection* arg0)
{
    if (arg0->remainingCount == 0) {
        return 1;
    }

    --arg0->remainingCount;
    return arg0->actorIds[(u_int)(arg0->cursor++)];
}

// decomp: BATTLE.PRG func_800BB7C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2920)
int Event_ActorDialogAnchor(int arg0, SVECTOR* arg1)
{
    MATRIX sp10;
    long sp30[5];

    arg1->vx = 0;
    arg1->vy = -0x3E8;
    arg1->vz = 0;
    Actors_ActorTaggedBoneMatrix(arg0, 1, &sp10, &sp30[2]);
    Core_Gte_SetRotMatrix(&sp10);
    Core_Gte_SetTransMatrix(&sp10);
    Core_Gte_RotTransPers(arg1, sp30, &sp30[3], &sp30[4]);
    Actors_ProjectActorTaggedBone(arg0, 1, arg1, sp30[2]);
    arg1->vz = Core_Gte_Atan2(((short*)&sp30)[1] - arg1->vy, (short)sp30[0] - arg1->vx);
    return arg1->vz;
}

