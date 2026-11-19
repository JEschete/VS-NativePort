// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"

struct Field_MapScene;
struct Field_MpdCollisionHeader;
struct Field_MpdDoorRecord;
struct Field_MpdEnvironment;
struct Field_MpdFloorTrigger;
struct Field_MpdLockTrigger;
struct Field_MpdMovingPlatform;
struct Field_MpdSectionE;
struct Field_MpdTileCell;
struct Field_MpdViewTrigger;
struct Field_RoomRecord;
struct Field_RoomTint;
struct Field_ZoneBounds;

// decomp: D_800F1BB0_t (src/BATTLE/BATTLE.PRG/146C.c:168)
struct Field_RoomTint {
    signed char red;
    signed char green;
    signed char blue;
    char reserved03;
    short strength;
    short isActive;
};
static_assert(sizeof(Field_RoomTint) == 0x8, "Field_RoomTint must keep its PS1 layout");

// decomp: _mpdRoomSection (src/BATTLE/BATTLE.PRG/146C.h:270)
struct Field_RoomRecord {
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
    void* lighting; /* a section pointer (filled with the others) */
    void* section6; /* a section pointer (filled with the others) */
    void* section7; /* a section pointer (filled with the others) */
    void* section8; /* a section pointer (filled with the others) */
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
    VS::Items::TreasureChest* containers;
    u_int* soundData;
    void* section15; /* a section pointer (filled with the others) */
    void* section16; /* a section pointer (filled with the others) */
    void* section17; /* a section pointer (filled with the others) */
    Field_ZoneBounds* zoneBounds;
};

extern void* Field_roomPrimitiveRangeEnd;
extern void* Field_roomPrimitiveRangeStart;
