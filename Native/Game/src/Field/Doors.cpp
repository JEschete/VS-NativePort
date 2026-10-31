// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Core/Memory/WordCopy.h"
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
short Field_lockedDoorIndexRanges[54]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E84AC (D_800E84AC@BATTLE/BATTLE.PRG)
char Field_lockedDoorRoomPairs[23][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F18E8 (D_800F18E8@BATTLE/BATTLE.PRG)
Field_DoorFlagIdWord Field_snapshotDoorFlagSource;

// decomp: D_800F1AB0 (D_800F1AB0@BATTLE/BATTLE.PRG)
Field_MpdDoorRecord Field_pendingDestinationDoor;

// decomp: vs_battle_doorEntered (vs_battle_doorEntered@BATTLE/BATTLE.PRG)
short Field_enteredDoorId;

// decomp: D_800F1CC8 (D_800F1CC8@BATTLE/BATTLE.PRG)
Field_MpdDoorRecord Field_entranceDoorRecord;

// decomp: BATTLE.PRG vs_battle_setDoorEntered (src/BATTLE/BATTLE.PRG/146C.c:15751)
void Field_StoreEntranceDoorForReload(void)
{
    Audio_Seq_unusedStateFlags.doorEntered = Field_enteredDoorId + 1;
}

// decomp: BATTLE.PRG func_8008B8F8 (src/BATTLE/BATTLE.PRG/146C.c:16127)
void Field_SetEntranceDoorRecord(char (*entrance_record)[12])
{
    if (entrance_record != NULL) {
        int doorId;
        Core_Memory_CopyAlignedWords(&Field_entranceDoorRecord, entrance_record, sizeof Field_entranceDoorRecord);
        doorId = Field_SceneDoorId((*entrance_record)[8]);
        Field_enteredDoorId = doorId;
    } else {
        Core_Memory_ZeroAlignedWords(&Field_entranceDoorRecord, sizeof Field_entranceDoorRecord);
        Field_enteredDoorId = -1;
    }
}

// decomp: BATTLE.PRG func_8008B960 (src/BATTLE/BATTLE.PRG/146C.c:16140)
void Field_FindEntranceDoorNearTile(int query_tile_x, int query_tile_z, int query_floor_layer)
{
    int door_count_or_scene_door_id;
    int j;
    Field_MpdDoorRecord* entrance_door_cursor;
    int i;

    if (Field_roomData.doors != 0) {
        door_count_or_scene_door_id = Field_roomData.header.doorsLength / 12;
        entrance_door_cursor = Field_roomData.doors;
        for (i = 0; i < door_count_or_scene_door_id; ++i, ++entrance_door_cursor) {
            if ((query_floor_layer >= 2) && ((&entrance_door_cursor->tileLocation)->floorLayer == query_floor_layer)) {
                break;
            }
            if ((&entrance_door_cursor->tileLocation)->floorLayer == query_floor_layer) {
                for (j = 0; j < 9; ++j) {
                    int neighbor_tile_radius = 1;
                    if (((&entrance_door_cursor->tileLocation)->tileX
                            == ((query_tile_x - neighbor_tile_radius) + (j - ((j / 3) * 3))))
                        && ((&entrance_door_cursor->tileLocation)->tileZ == ((query_tile_z - neighbor_tile_radius) + (j / 3)))) {
                        break;
                    }
                }
                if (j != 9) {
                    break;
                }
            }
        }
        if (i == door_count_or_scene_door_id) {
            entrance_door_cursor = NULL;
        }
    } else {
        entrance_door_cursor = NULL;
    }

    if (entrance_door_cursor != NULL) {
        Core_Memory_CopyAlignedWords(&Field_entranceDoorRecord, entrance_door_cursor, sizeof Field_entranceDoorRecord);
        door_count_or_scene_door_id = Field_SceneDoorId(entrance_door_cursor->doorId);
        Field_enteredDoorId = door_count_or_scene_door_id;
    } else {
        Core_Memory_ZeroAlignedWords(&Field_entranceDoorRecord, sizeof Field_entranceDoorRecord);
        Field_enteredDoorId = -1;
    }
}

// decomp: BATTLE.PRG func_8008BC04 (src/BATTLE/BATTLE.PRG/146C.c:16215)
Field_MpdLockTrigger* Field_InteractiveDoorAtTile(int query_tile_x, int query_tile_z, int query_floor_layer)
{
    int lock_trigger_count;
    Field_MpdLockTrigger* lock_trigger_cursor;
    int i;

    if (Field_roomData.lockTriggers != 0) {
        lock_trigger_count = Field_roomData.header.lockTriggersLength / 20;
        lock_trigger_cursor = Field_roomData.lockTriggers;
        if (lock_trigger_cursor->trigger.activation != 0) {
            for (i = 0; i < lock_trigger_count; ++i, ++lock_trigger_cursor) {
                if ((lock_trigger_cursor->trigger.tileX == query_tile_x) && (lock_trigger_cursor->trigger.tileZ == query_tile_z)
                    && (lock_trigger_cursor->trigger.floorLayer == query_floor_layer)) {
                    return lock_trigger_cursor;
                }
                switch (lock_trigger_cursor->trigger.kind - 4) {
                case 0:
                    if ((lock_trigger_cursor->trigger.tileX == query_tile_x) && ((lock_trigger_cursor->trigger.tileZ - 1) == query_tile_z)
                        && (lock_trigger_cursor->trigger.floorLayer == query_floor_layer)) {
                        return lock_trigger_cursor;
                    }
                    break;
                case 1:
                    if (((lock_trigger_cursor->trigger.tileX + 1) == query_tile_x) && (lock_trigger_cursor->trigger.tileZ == query_tile_z)
                        && (lock_trigger_cursor->trigger.floorLayer == query_floor_layer)) {
                        return lock_trigger_cursor;
                    }
                    break;
                case 2:
                    if ((lock_trigger_cursor->trigger.tileX == query_tile_x) && ((lock_trigger_cursor->trigger.tileZ + 1) == query_tile_z)
                        && (lock_trigger_cursor->trigger.floorLayer == query_floor_layer)) {
                        return lock_trigger_cursor;
                    }
                    break;
                case 3:
                    if (((lock_trigger_cursor->trigger.tileX - 1) == query_tile_x) && (lock_trigger_cursor->trigger.tileZ == query_tile_z)
                        && (lock_trigger_cursor->trigger.floorLayer == query_floor_layer)) {
                        return lock_trigger_cursor;
                    }
                    break;
                }
            }
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008BD74 (src/BATTLE/BATTLE.PRG/146C.c:16262)
void Field_ToggleDoorCollision(Field_MpdLockTrigger* door_trigger)
{
    Field_MpdTileCell* collision_cell;

    collision_cell = Field_CollisionCellAt(door_trigger->trigger.tileX, door_trigger->trigger.tileZ, door_trigger->trigger.floorLayer);
    if (collision_cell != NULL) {
        collision_cell->doorPassability ^= 5;
    }
    collision_cell = NULL;
    switch (door_trigger->trigger.kind - 4) {
    case 0:
        collision_cell = Field_CollisionCellAt(door_trigger->trigger.tileX, door_trigger->trigger.tileZ - 1, door_trigger->trigger.floorLayer);
        break;
    case 1:
        collision_cell = Field_CollisionCellAt(door_trigger->trigger.tileX + 1, door_trigger->trigger.tileZ, door_trigger->trigger.floorLayer);
        break;
    case 2:
        collision_cell = Field_CollisionCellAt(door_trigger->trigger.tileX, door_trigger->trigger.tileZ + 1, door_trigger->trigger.floorLayer);
        break;
    case 3:
        collision_cell = Field_CollisionCellAt(door_trigger->trigger.tileX - 1, door_trigger->trigger.tileZ, door_trigger->trigger.floorLayer);
        break;
    }
    if ((door_trigger->trigger.kind >= 4) && (collision_cell != NULL)) {
        collision_cell->doorPassability ^= 5;
    }
}

// decomp: BATTLE.PRG func_8008BEBC (src/BATTLE/BATTLE.PRG/146C.c:16290)
int Field_BeginDoorToggle(Field_MpdLockTrigger* door_trigger)
{
    if (door_trigger->isTriggered == 0) {
        u_int key_or_sigil_id = (u_short)door_trigger->keyOrSigil;
        door_trigger->isTriggered = 1;
        if ((key_or_sigil_id - 0x1CA) < 7) {
            Field_MarkSigilDoorUnlocked((short)key_or_sigil_id - 0x1CA);
        }
        if (door_trigger->isOpen == 0) {
            door_trigger->delayFrames = 8;
            Field_QueueGimmickAnimation(3, door_trigger);
            return 1;
        }
        Field_StartDoorAnimation(door_trigger);
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008BF48 (src/BATTLE/BATTLE.PRG/146C.c:16309)
int Field_StartDoorAnimation(Field_MpdLockTrigger* door_trigger)
{
    Field_MpdLockTrigger* lock_trigger_table;

    if (door_trigger->isOpen == 0) {
        Audio_PlaySfxCentered(0x7E, 0x1D);
        Audio_PlaySfxCentered(0x7E, 0x1E);
    } else {
        Audio_PlaySfxCentered(0x7E, 0x16);
        Audio_PlaySfxCentered(0x7E, 0x17);
    }
    Field_StartRoomEffect(door_trigger->trigger.lockState.lock.roomEffect);
    if (door_trigger->isOpen != 0) {
        Field_ToggleDoorCollision(door_trigger);
    }
    if (door_trigger->linkedLockIndex != -1) {
        lock_trigger_table = Field_roomData.lockTriggers;
        if (lock_trigger_table[door_trigger->linkedLockIndex].trigger.activation != 0) {
            Field_StartRoomEffect(lock_trigger_table[door_trigger->linkedLockIndex].trigger.lockState.lock.roomEffect);
            if (lock_trigger_table[door_trigger->linkedLockIndex].isOpen != 0) {
                Field_ToggleDoorCollision(&lock_trigger_table[door_trigger->linkedLockIndex]);
            }
            lock_trigger_table[door_trigger->linkedLockIndex].isTriggered = 1;
        }
    }
    Field_QueueGimmickAnimation(1, door_trigger);
    return 1;
}

// decomp: BATTLE.PRG func_8008C1C8 (src/BATTLE/BATTLE.PRG/146C.c:16389)
int Field_FinishDoorAnimation(Field_MpdLockTrigger* door_trigger)
{
    if (Field_IsRoomEffectActive(door_trigger->trigger.lockState.lock.roomEffect) != 0) {
        return 0;
    }
    if (door_trigger->isOpen == 0) {
        Field_ToggleDoorCollision(door_trigger);
    }
    door_trigger->isTriggered = 0;
    door_trigger->isOpen = door_trigger->isOpen == 0;
    if (door_trigger->linkedLockIndex != -1) {
        Field_MpdLockTrigger* p = Field_roomData.lockTriggers;
        if (p[door_trigger->linkedLockIndex].trigger.activation != 0) {
            if (p[door_trigger->linkedLockIndex].isOpen == 0) {
                Field_ToggleDoorCollision(&p[door_trigger->linkedLockIndex]);
            }
            p[door_trigger->linkedLockIndex].isOpen = p[door_trigger->linkedLockIndex].isOpen == 0;
            p[door_trigger->linkedLockIndex].isTriggered = 0;
        }
    }
    return 1;
}

// decomp: BATTLE.PRG _getDoorId (src/BATTLE/BATTLE.PRG/146C.c:16954)
int Field_SceneDoorId(int door)
{
    int* scene_section_word_cursor;

    if (Field_roomData.minimapScene != NULL) {
        scene_section_word_cursor = (int*)Field_roomData.minimapScene
                + (Field_roomData.minimapScene->roomCount * 3 + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor * 2) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        if ((door < *scene_section_word_cursor++) && (door >= 0)) {
            scene_section_word_cursor += door;
            return ((char*)scene_section_word_cursor)[1];
        }
    }
    return -1;
}

// decomp: BATTLE.PRG vs_battle_setDoorEntered2 (src/BATTLE/BATTLE.PRG/146C.c:16974)
void Field_MarkEntranceDoorOnSceneMap(int entered_door_index)
{
    int scene_door_count;
    int i;

    Field_enteredDoorId = entered_door_index;

    if (Field_roomData.minimapScene != NULL) {
        int* scene_section_word_cursor = (int*)Field_roomData.minimapScene;
        scene_section_word_cursor += Field_roomData.minimapScene->roomCount * 3 + 1;
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor * 2) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_section_word_cursor = scene_section_word_cursor + ((*scene_section_word_cursor) + 1);
        scene_door_count = *scene_section_word_cursor++;

        for (i = 0; i < scene_door_count; ++i) {
            if (i == entered_door_index) {
                ((char*)scene_section_word_cursor)[2] |= 0x10;
            }
            ++scene_section_word_cursor;
        }
    }
}

// decomp: BATTLE.PRG func_8008E3B8 (src/BATTLE/BATTLE.PRG/146C.c:17296)
Field_MpdLockTrigger* Field_RoomInteractiveDoors(int* output_door_count)
{
    if (Field_roomData.lockTriggers != 0) {
        *output_door_count =
            Field_roomData.header.lockTriggersLength / sizeof *Field_roomData.lockTriggers;
        return Field_roomData.lockTriggers;
    }
    *output_door_count = 0;
    return 0;
}

// decomp: BATTLE.PRG get_D_800E84AC (src/BATTLE/BATTLE.PRG/146C.c:17397)
char Field_SigilDoorEndpointCoordinate(int door_pair_index, int endpoint_component) { return Field_lockedDoorRoomPairs[door_pair_index][endpoint_component]; }

// decomp: BATTLE.PRG func_8008E6DC (src/BATTLE/BATTLE.PRG/146C.c:17399)
void Field_MarkSigilDoorUnlocked(int sigil_index)
{
    int mapId;
    int zoneId;
    int i;
    Field_MapRoom* room;

    room = Field_roomData.minimapScene->rooms;
    zoneId = room->zoneId;
    mapId = room->mapId;

    for (i = Field_lockedDoorIndexRanges[sigil_index]; i < Field_lockedDoorIndexRanges[sigil_index + 1]; ++i) {
        if (((Field_SigilDoorEndpointCoordinate(i, 0) == zoneId) && (Field_SigilDoorEndpointCoordinate(i, 1) == mapId))
            || ((Field_SigilDoorEndpointCoordinate(i, 2) == zoneId) && (Field_SigilDoorEndpointCoordinate(i, 3) == mapId))) {
            Audio_Seq_unusedStateFlags.opaque186[i] = 1;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_8008E7BC (src/BATTLE/BATTLE.PRG/146C.c:17419)
int Field_IsRoomSigilDoorUnlocked(int zone_id, int map_id, u_int sigil_index)
{
    int i;
    for (i = Field_lockedDoorIndexRanges[sigil_index]; i < Field_lockedDoorIndexRanges[sigil_index + 1]; ++i) {
        if (((Field_SigilDoorEndpointCoordinate(i, 0) == zone_id) && (Field_SigilDoorEndpointCoordinate(i, 1) == map_id))
            || ((Field_SigilDoorEndpointCoordinate(i, 2) == zone_id) && (Field_SigilDoorEndpointCoordinate(i, 3) == map_id))) {
            return Audio_Seq_unusedStateFlags.opaque186[i];
        }
    }
    return 0;
}

}  // extern "C"
