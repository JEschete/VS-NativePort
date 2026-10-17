// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Core/Memory/Heap.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "State/StateFlags.h"

// decomp: D_800E849C (D_800E849C@BATTLE/BATTLE.PRG)
short Field_lockedDoorIndexRanges[54]; /* data: contents loaded in Step 5 */

// decomp: D_800E84AC (D_800E84AC@BATTLE/BATTLE.PRG)
char Field_lockedDoorRoomPairs[23][4]; /* data: contents loaded in Step 5 */

// decomp: D_800F18E8 (D_800F18E8@BATTLE/BATTLE.PRG)
Field_DoorFlagIdWord Field_snapshotDoorFlagSource;

// decomp: D_800F1AB0 (D_800F1AB0@BATTLE/BATTLE.PRG)
Field_MpdDoorRecord Field_pendingDestinationDoor;

// decomp: vs_battle_doorEntered (vs_battle_doorEntered@BATTLE/BATTLE.PRG)
short Field_enteredDoorId;

// decomp: D_800F1CC8 (D_800F1CC8@BATTLE/BATTLE.PRG)
Field_MpdDoorRecord Field_entranceDoorRecord;

// decomp: D_800F1CCA (D_800F1CCA@BATTLE/BATTLE.PRG)
u_short Field_entranceDoorPlatformFlags;

// decomp: D_800F1D08 (D_800F1D08@BATTLE/BATTLE.PRG)
Field_DoorMatchList Field_matchingDoorRecords;

// decomp: BATTLE.PRG vs_battle_setDoorEntered (src/BATTLE/BATTLE.PRG/146C.c:15751)
void Field_StoreEntranceDoorForReload(void)
{
    Audio_Seq_unusedStateFlags.doorEntered = Field_enteredDoorId + 1;
}

// decomp: BATTLE.PRG func_8008B8F8 (src/BATTLE/BATTLE.PRG/146C.c:16127)
void Field_SetEntranceDoorRecord(char (*arg0)[12])
{
    if (arg0 != NULL) {
        int doorId;
        Core_Memory_CopyAlignedWords(&Field_entranceDoorRecord, arg0, sizeof Field_entranceDoorRecord);
        doorId = Field_SceneDoorId((*arg0)[8]);
        Field_enteredDoorId = doorId;
    } else {
        Core_Memory_ZeroAlignedWords(&Field_entranceDoorRecord, sizeof Field_entranceDoorRecord);
        Field_enteredDoorId = -1;
    }
}

// decomp: BATTLE.PRG func_8008B960 (src/BATTLE/BATTLE.PRG/146C.c:16140)
void Field_FindEntranceDoorNearTile(int arg0, int arg1, int arg2)
{
    int temp_t4;
    int j;
    Field_MpdDoorRecord* var_s0;
    int i;

    if (Field_roomData.doors != 0) {
        temp_t4 = Field_roomData.header.doorsLength / 12;
        var_s0 = Field_roomData.doors;
        for (i = 0; i < temp_t4; ++i, ++var_s0) {
            if ((arg2 >= 2) && ((&var_s0->tileLocation)->floorLayer == arg2)) {
                break;
            }
            if ((&var_s0->tileLocation)->floorLayer == arg2) {
                for (j = 0; j < 9; ++j) {
                    int new_var = 1;
                    if (((&var_s0->tileLocation)->tileX
                            == ((arg0 - new_var) + (j - ((j / 3) * 3))))
                        && ((&var_s0->tileLocation)->tileZ == ((arg1 - new_var) + (j / 3)))) {
                        break;
                    }
                }
                if (j != 9) {
                    break;
                }
            }
        }
        if (i == temp_t4) {
            var_s0 = NULL;
        }
    } else {
        var_s0 = NULL;
    }

    if (var_s0 != NULL) {
        Core_Memory_CopyAlignedWords(&Field_entranceDoorRecord, var_s0, sizeof Field_entranceDoorRecord);
        temp_t4 = Field_SceneDoorId(var_s0->doorId);
        Field_enteredDoorId = temp_t4;
    } else {
        Core_Memory_ZeroAlignedWords(&Field_entranceDoorRecord, sizeof Field_entranceDoorRecord);
        Field_enteredDoorId = -1;
    }
}

// decomp: BATTLE.PRG func_8008BAC8 (src/BATTLE/BATTLE.PRG/146C.c:16185)
Field_DoorMatchList* Field_TransitionDoorsAtTile(int arg0, int arg1, int arg2)
{
    Field_MpdDoorRecord* var_s0;
    int temp_s1;
    int i;
    Field_MpdDoorRecord* new_var;

    if (Field_roomData.doors != NULL) {
        var_s0 = Field_roomData.doors;
        Field_matchingDoorRecords.count = 0;
        temp_s1 = Field_roomData.header.doorsLength / 12;

        Audio_Seq_unusedStateFlags.opaqueAC[7] = Core_Random_Rand();

        for (i = 0; i < temp_s1; ++i, ++var_s0) {
            new_var = var_s0;
            if (((arg2 >= 2) && ((&new_var->tileLocation)->floorLayer == arg2))
                || (((((&var_s0->tileLocation)->tileX) == arg0))
                    && ((((&var_s0->tileLocation)->tileZ)) == arg1)
                    && ((((&var_s0->tileLocation)->floorLayer)) == arg2))) {
                do {
                    Field_matchingDoorRecords.records[Field_matchingDoorRecords.count++] = var_s0;
                } while (0);
            }
        }
        return &Field_matchingDoorRecords;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008BC04 (src/BATTLE/BATTLE.PRG/146C.c:16215)
Field_MpdLockTrigger* Field_InteractiveDoorAtTile(int arg0, int arg1, int arg2)
{
    int temp_t2;
    Field_MpdLockTrigger* var_t0;
    int i;

    if (Field_roomData.lockTriggers != 0) {
        temp_t2 = Field_roomData.header.lockTriggersLength / 20;
        var_t0 = Field_roomData.lockTriggers;
        if (var_t0->trigger.activation != 0) {
            for (i = 0; i < temp_t2; ++i, ++var_t0) {
                if ((var_t0->trigger.tileX == arg0) && (var_t0->trigger.tileZ == arg1)
                    && (var_t0->trigger.floorLayer == arg2)) {
                    return var_t0;
                }
                switch (var_t0->trigger.kind - 4) {
                case 0:
                    if ((var_t0->trigger.tileX == arg0) && ((var_t0->trigger.tileZ - 1) == arg1)
                        && (var_t0->trigger.floorLayer == arg2)) {
                        return var_t0;
                    }
                    break;
                case 1:
                    if (((var_t0->trigger.tileX + 1) == arg0) && (var_t0->trigger.tileZ == arg1)
                        && (var_t0->trigger.floorLayer == arg2)) {
                        return var_t0;
                    }
                    break;
                case 2:
                    if ((var_t0->trigger.tileX == arg0) && ((var_t0->trigger.tileZ + 1) == arg1)
                        && (var_t0->trigger.floorLayer == arg2)) {
                        return var_t0;
                    }
                    break;
                case 3:
                    if (((var_t0->trigger.tileX - 1) == arg0) && (var_t0->trigger.tileZ == arg1)
                        && (var_t0->trigger.floorLayer == arg2)) {
                        return var_t0;
                    }
                    break;
                }
            }
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008BD74 (src/BATTLE/BATTLE.PRG/146C.c:16262)
void Field_ToggleDoorCollision(Field_MpdLockTrigger* arg0)
{
    Field_MpdTileCell* var_a0;

    var_a0 = Field_CollisionCellAt(arg0->trigger.tileX, arg0->trigger.tileZ, arg0->trigger.floorLayer);
    if (var_a0 != NULL) {
        var_a0->doorPassability ^= 5;
    }
    var_a0 = NULL;
    switch (arg0->trigger.kind - 4) {
    case 0:
        var_a0 = Field_CollisionCellAt(arg0->trigger.tileX, arg0->trigger.tileZ - 1, arg0->trigger.floorLayer);
        break;
    case 1:
        var_a0 = Field_CollisionCellAt(arg0->trigger.tileX + 1, arg0->trigger.tileZ, arg0->trigger.floorLayer);
        break;
    case 2:
        var_a0 = Field_CollisionCellAt(arg0->trigger.tileX, arg0->trigger.tileZ + 1, arg0->trigger.floorLayer);
        break;
    case 3:
        var_a0 = Field_CollisionCellAt(arg0->trigger.tileX - 1, arg0->trigger.tileZ, arg0->trigger.floorLayer);
        break;
    }
    if ((arg0->trigger.kind >= 4) && (var_a0 != NULL)) {
        var_a0->doorPassability ^= 5;
    }
}

// decomp: BATTLE.PRG func_8008BEBC (src/BATTLE/BATTLE.PRG/146C.c:16290)
int Field_BeginDoorToggle(Field_MpdLockTrigger* arg0)
{
    if (arg0->isTriggered == 0) {
        u_int temp_a0 = (u_short)arg0->keyOrSigil;
        arg0->isTriggered = 1;
        if ((temp_a0 - 0x1CA) < 7) {
            Field_MarkSigilDoorUnlocked((short)temp_a0 - 0x1CA);
        }
        if (arg0->isOpen == 0) {
            arg0->delayFrames = 8;
            Field_QueueGimmickAnimation(3, arg0);
            return 1;
        }
        Field_StartDoorAnimation(arg0);
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008BF48 (src/BATTLE/BATTLE.PRG/146C.c:16309)
int Field_StartDoorAnimation(Field_MpdLockTrigger* arg0)
{
    Field_MpdLockTrigger* temp_s0;

    if (arg0->isOpen == 0) {
        Audio_PlaySfxCentered(0x7E, 0x1D);
        Audio_PlaySfxCentered(0x7E, 0x1E);
    } else {
        Audio_PlaySfxCentered(0x7E, 0x16);
        Audio_PlaySfxCentered(0x7E, 0x17);
    }
    Field_StartRoomEffect(arg0->trigger.lockState.lock.roomEffect);
    if (arg0->isOpen != 0) {
        Field_ToggleDoorCollision(arg0);
    }
    if (arg0->linkedLockIndex != -1) {
        temp_s0 = Field_roomData.lockTriggers;
        if (temp_s0[arg0->linkedLockIndex].trigger.activation != 0) {
            Field_StartRoomEffect(temp_s0[arg0->linkedLockIndex].trigger.lockState.lock.roomEffect);
            if (temp_s0[arg0->linkedLockIndex].isOpen != 0) {
                Field_ToggleDoorCollision(&temp_s0[arg0->linkedLockIndex]);
            }
            temp_s0[arg0->linkedLockIndex].isTriggered = 1;
        }
    }
    Field_QueueGimmickAnimation(1, arg0);
    return 1;
}

// decomp: BATTLE.PRG func_8008C1C8 (src/BATTLE/BATTLE.PRG/146C.c:16389)
int Field_FinishDoorAnimation(Field_MpdLockTrigger* arg0)
{
    if (Field_IsRoomEffectActive(arg0->trigger.lockState.lock.roomEffect) != 0) {
        return 0;
    }
    if (arg0->isOpen == 0) {
        Field_ToggleDoorCollision(arg0);
    }
    arg0->isTriggered = 0;
    arg0->isOpen = arg0->isOpen == 0;
    if (arg0->linkedLockIndex != -1) {
        Field_MpdLockTrigger* p = Field_roomData.lockTriggers;
        if (p[arg0->linkedLockIndex].trigger.activation != 0) {
            if (p[arg0->linkedLockIndex].isOpen == 0) {
                Field_ToggleDoorCollision(&p[arg0->linkedLockIndex]);
            }
            p[arg0->linkedLockIndex].isOpen = p[arg0->linkedLockIndex].isOpen == 0;
            p[arg0->linkedLockIndex].isTriggered = 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG _getDoorId (src/BATTLE/BATTLE.PRG/146C.c:16954)
int Field_SceneDoorId(int door)
{
    int* temp_v1;

    if (Field_roomData.minimapScene != NULL) {
        temp_v1 = (int*)Field_roomData.minimapScene
                + (Field_roomData.minimapScene->roomCount * 3 + 1);
        temp_v1 = temp_v1 + ((*temp_v1 * 2) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        if ((door < *temp_v1++) && (door >= 0)) {
            temp_v1 += door;
            return ((char*)temp_v1)[1];
        }
    }
    return -1;
}

// decomp: BATTLE.PRG vs_battle_setDoorEntered2 (src/BATTLE/BATTLE.PRG/146C.c:16974)
void Field_MarkEntranceDoorOnSceneMap(int arg0)
{
    int temp_a3;
    int i;

    Field_enteredDoorId = arg0;

    if (Field_roomData.minimapScene != NULL) {
        int* temp_v1 = (int*)Field_roomData.minimapScene;
        temp_v1 += Field_roomData.minimapScene->roomCount * 3 + 1;
        temp_v1 = temp_v1 + ((*temp_v1 * 2) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_v1 = temp_v1 + ((*temp_v1) + 1);
        temp_a3 = *temp_v1++;

        for (i = 0; i < temp_a3; ++i) {
            if (i == arg0) {
                ((char*)temp_v1)[2] |= 0x10;
            }
            ++temp_v1;
        }
    }
}

// decomp: BATTLE.PRG func_8008E3B8 (src/BATTLE/BATTLE.PRG/146C.c:17296)
Field_MpdLockTrigger* Field_RoomInteractiveDoors(int* arg0)
{
    if (Field_roomData.lockTriggers != 0) {
        *arg0 =
            Field_roomData.header.lockTriggersLength / sizeof *Field_roomData.lockTriggers;
        return Field_roomData.lockTriggers;
    }
    *arg0 = 0;
    return 0;
}

// decomp: BATTLE.PRG get_D_800E84AC (src/BATTLE/BATTLE.PRG/146C.c:17397)
char Field_SigilDoorEndpointCoordinate(int arg0, int arg1) { return Field_lockedDoorRoomPairs[arg0][arg1]; }

// decomp: BATTLE.PRG func_8008E6DC (src/BATTLE/BATTLE.PRG/146C.c:17399)
void Field_MarkSigilDoorUnlocked(int arg0)
{
    int mapId;
    int zoneId;
    int i;
    Field_MapRoom* room;

    room = Field_roomData.minimapScene->rooms;
    zoneId = room->zoneId;
    mapId = room->mapId;

    for (i = Field_lockedDoorIndexRanges[arg0]; i < Field_lockedDoorIndexRanges[arg0 + 1]; ++i) {
        if (((Field_SigilDoorEndpointCoordinate(i, 0) == zoneId) && (Field_SigilDoorEndpointCoordinate(i, 1) == mapId))
            || ((Field_SigilDoorEndpointCoordinate(i, 2) == zoneId) && (Field_SigilDoorEndpointCoordinate(i, 3) == mapId))) {
            Audio_Seq_unusedStateFlags.opaque186[i] = 1;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_8008E7BC (src/BATTLE/BATTLE.PRG/146C.c:17419)
int Field_IsRoomSigilDoorUnlocked(int arg0, int arg1, u_int arg2)
{
    int i;
    for (i = Field_lockedDoorIndexRanges[arg2]; i < Field_lockedDoorIndexRanges[arg2 + 1]; ++i) {
        if (((Field_SigilDoorEndpointCoordinate(i, 0) == arg0) && (Field_SigilDoorEndpointCoordinate(i, 1) == arg1))
            || ((Field_SigilDoorEndpointCoordinate(i, 2) == arg0) && (Field_SigilDoorEndpointCoordinate(i, 3) == arg1))) {
            return Audio_Seq_unusedStateFlags.opaque186[i];
        }
    }
    return 0;
}

