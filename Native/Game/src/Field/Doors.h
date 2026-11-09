// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_DOORS_H_TYPES
#define VS_FIELD_DOORS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_DoorFlagIdWord Field_DoorFlagIdWord;
typedef struct Field_DoorTileLocation Field_DoorTileLocation;

// decomp: D_800F1CCA (D_800F1CCA@BATTLE/BATTLE.PRG): a view into another global
#define Field_entranceDoorPlatformFlags (*(u_short*)(&Field_entranceDoorRecord.tileLocation))

// decomp: D_800F18E8_t (src/BATTLE/BATTLE.PRG/146C.h:731)
typedef struct Field_DoorFlagIdWord {
    u_short requiredFlagId : 10;
    u_short reservedBits10 : 6;
} Field_DoorFlagIdWord;
_Static_assert(sizeof(Field_DoorFlagIdWord) == 0x2, "Field_DoorFlagIdWord must keep its PS1 layout");

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

#endif // VS_FIELD_DOORS_H_DECLS
