// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ROOM_H_TYPES
#define VS_FIELD_ROOM_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_MapScene Field_MapScene;
typedef struct Field_MpdCollisionHeader Field_MpdCollisionHeader;
typedef struct Field_MpdDoorRecord Field_MpdDoorRecord;
typedef struct Field_MpdEnvironment Field_MpdEnvironment;
typedef struct Field_MpdFloorTrigger Field_MpdFloorTrigger;
typedef struct Field_MpdLockTrigger Field_MpdLockTrigger;
typedef struct Field_MpdMovingPlatform Field_MpdMovingPlatform;
typedef struct Field_MpdSectionE Field_MpdSectionE;
typedef struct Field_MpdTileCell Field_MpdTileCell;
typedef struct Field_MpdViewTrigger Field_MpdViewTrigger;
typedef struct Field_RoomRecord Field_RoomRecord;
typedef struct Field_RoomTint Field_RoomTint;
typedef struct Field_ZoneBounds Field_ZoneBounds;
typedef struct Items_TreasureChest Items_TreasureChest;

// decomp: D_800F1BB0_t (src/BATTLE/BATTLE.PRG/146C.c:168)
typedef struct Field_RoomTint {
    signed char red;
    signed char green;
    signed char blue;
    char reserved03;
    short strength;
    short isActive;
} Field_RoomTint;

// decomp: _mpdRoomSection (src/BATTLE/BATTLE.PRG/146C.h:270)
typedef struct Field_RoomRecord {
    char isLoaded;
    char displayFlag;
    short roomIndex;
    struct {
        u_int geometryLength;
        u_int collisionMapLength;
        u_int tileCellsLength;
        u_int doorsLength;
        u_int lightingLength;
        u_int section6Length;
        u_int section7Length;
        u_int section8Length;
        u_int floorTriggersLength;
        u_int lockTriggersLength;
        u_int environmentLength;
        u_int textureEffectsLength;
        u_int effectScriptsLength;
        u_int sectionELength;
        u_int minimapSceneLength;
        u_int tiledBackgroundLength;
        u_int viewTriggersLength;
        u_int movingPlatformsLength;
        u_int containersLength;
        u_int soundDataLength;
        u_int section15Length;
        u_int section16Length;
        u_int section17Length;
        u_int zoneBoundsLength;
    } header;
    int* geometry;
    Field_MpdCollisionHeader* collisionMap;
    Field_MpdTileCell* tileCells;
    Field_MpdDoorRecord* doors;
    int lighting;
    int section6;
    int section7;
    int section8;
    Field_MpdFloorTrigger* floorTriggers;
    Field_MpdLockTrigger* lockTriggers;
    Field_MpdEnvironment* environment;
    void* textureEffects;
    void* effectScripts;
    Field_MpdSectionE* sectionE;
    Field_MapScene* minimapScene;
    u_short* tiledBackground;
    Field_MpdViewTrigger* viewTriggers;
    Field_MpdMovingPlatform* movingPlatforms;
    Items_TreasureChest* containers;
    u_int* soundData;
    int section15;
    int section16;
    int section17;
    Field_ZoneBounds* zoneBounds;
} Field_RoomRecord;

#endif // VS_FIELD_ROOM_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ROOM_H_DECLS)
#define VS_FIELD_ROOM_H_DECLS

extern void* Field_roomPrimitiveRangeEnd;
extern void* Field_roomPrimitiveRangeStart;

#endif // VS_FIELD_ROOM_H_DECLS
