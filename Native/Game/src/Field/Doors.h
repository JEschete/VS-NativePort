// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_DOORS_H_TYPES
#define VS_FIELD_DOORS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_DoorFlagIdWord Field_DoorFlagIdWord;
typedef struct Field_DoorMatchList Field_DoorMatchList;
typedef struct Field_DoorTileLocation Field_DoorTileLocation;
typedef struct Field_MpdDoorRecord Field_MpdDoorRecord;

// decomp: D_800F1CCA (D_800F1CCA@BATTLE/BATTLE.PRG): a view into another global
#define Field_entranceDoorPlatformFlags (*(u_short*)(&Field_entranceDoorRecord.tileLocation))

// decomp: D_800F18E8_t (src/BATTLE/BATTLE.PRG/146C.h:731)
typedef struct Field_DoorFlagIdWord {
    u_short requiredFlagId : 10;
    u_short reservedBits10 : 6;
} Field_DoorFlagIdWord;
_Static_assert(sizeof(Field_DoorFlagIdWord) == 0x2, "Field_DoorFlagIdWord must keep its PS1 layout");

// decomp: D_800F1D08_t (src/BATTLE/BATTLE.PRG/146C.c:280)
typedef struct Field_DoorMatchList {
    int count;
    Field_MpdDoorRecord* records[0];
} Field_DoorMatchList;

// decomp: _mpdRoomDoorSection2 (src/SLUS_010.40/main.h:429)
typedef struct Field_DoorTileLocation {
    u_short tileX : 5;
    u_short tileZ : 5;
    u_short floorLayer : 3;
    u_short entryDirection : 3;
} Field_DoorTileLocation;
_Static_assert(sizeof(Field_DoorTileLocation) == 0x2, "Field_DoorTileLocation must keep its PS1 layout");

#endif // VS_FIELD_DOORS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_DOORS_H_DECLS)
#define VS_FIELD_DOORS_H_DECLS
#include "Field/Mpd.h"

extern short Field_lockedDoorIndexRanges[54];
extern char Field_lockedDoorRoomPairs[23][4];
extern Field_DoorFlagIdWord Field_snapshotDoorFlagSource;
extern Field_MpdDoorRecord Field_pendingDestinationDoor;
extern short Field_enteredDoorId;
extern Field_MpdDoorRecord Field_entranceDoorRecord;
extern Field_DoorMatchList Field_matchingDoorRecords;
void Field_StoreEntranceDoorForReload(void);
void Field_SetEntranceDoorRecord(char (*arg0)[12]);
void Field_FindEntranceDoorNearTile(int arg0, int arg1, int arg2);
Field_DoorMatchList* Field_TransitionDoorsAtTile(int arg0, int arg1, int arg2);
Field_MpdLockTrigger* Field_InteractiveDoorAtTile(int arg0, int arg1, int arg2);
void Field_ToggleDoorCollision(Field_MpdLockTrigger* arg0);
int Field_BeginDoorToggle(Field_MpdLockTrigger* arg0);
int Field_StartDoorAnimation(Field_MpdLockTrigger* arg0);
int Field_FinishDoorAnimation(Field_MpdLockTrigger* arg0);
int Field_SceneDoorId(int door);
void Field_MarkEntranceDoorOnSceneMap(int arg0);
Field_MpdLockTrigger* Field_RoomInteractiveDoors(int* arg0);
char Field_SigilDoorEndpointCoordinate(int arg0, int arg1);
void Field_MarkSigilDoorUnlocked(int arg0);
int Field_IsRoomSigilDoorUnlocked(int arg0, int arg1, u_int arg2);

#endif // VS_FIELD_DOORS_H_DECLS
