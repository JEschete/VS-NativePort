// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_MPD_H_TYPES
#define VS_FIELD_MPD_H_TYPES
#include "Legacy/Psx.h"

struct Field_MpdCollisionHeader;
struct Field_MpdDoorRecord;
struct Field_MpdEnemySpawn;
struct Field_MpdEnvironment;
struct Field_MpdFloorTrigger;
struct Field_MpdGeometrySection;
struct Field_MpdHeader;
struct Field_MpdLockTrigger;
struct Field_MpdMovingPlatform;
struct Field_MpdObjectSpawn;
struct Field_MpdPolygonGroupHeader;
struct Field_MpdPolygonUvView;
struct Field_MpdQuad;
struct Field_MpdQuadColorView;
struct Field_MpdSectionE;
struct Field_MpdTileCell;
struct Field_MpdTriangle;
struct Field_MpdTriangleColorView;
struct Field_MpdViewTrigger;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_FIELD_MPD_H_SET_TYPES_ONLY
#endif
#include "Field/Doors.h"
#ifdef VS_FIELD_MPD_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_FIELD_MPD_H_SET_TYPES_ONLY
#endif

// decomp: _mpdEnemy (src/BATTLE/BATTLE.PRG/146C.h:689)
struct Field_MpdEnemySpawn {
    u_char kind;
    u_char index;
    u_char isHidden;
    u_char reserved03;
    u_char enemyId;
    u_char slot;
    u_char flagTest;
    u_char statTest;
    u_short flagValue;
    u_short statValue;
    u_char tileX;
    u_char direction;
    u_char tileZ;
    u_char height;
    u_char settings[8];
    u_char reserved18[2];
    u_short itemId;
    u_short item2Id;
    u_short item3Id;
    u_char itemCount;
    u_char item2Count;
    u_char item3Count;
    u_char facing;
    u_char actorStateByte;
    u_char variant;
    u_char reserved26;
    u_char sceneTest;
};
static_assert(sizeof(Field_MpdEnemySpawn) == 0x28, "Field_MpdEnemySpawn must keep its PS1 layout");

// decomp: _mpdGeometryHeader (src/BATTLE/BATTLE.PRG/146C.h:31)
struct Field_MpdPolygonGroupHeader {
    int reserved0;
    int reserved4;
    int reserved8;
    int reservedC;
    int reserved10;
    int reserved14;
    int reserved18;
    int reserved1C;
    int reserved20;
    int reserved24;
    int reserved28;
    int reserved2C;
    int reserved30;
    int reserved34;
    int reserved38;
    int reserved3C;
};
static_assert(sizeof(Field_MpdPolygonGroupHeader) == 0x40, "Field_MpdPolygonGroupHeader must keep its PS1 layout");

// decomp: _mpdGeometrySection (src/BATTLE/BATTLE.PRG/146C.h:64)
struct Field_MpdGeometrySection {
    int groupCount;
    Field_MpdPolygonGroupHeader groupHeaders[0];
};
static_assert(sizeof(Field_MpdGeometrySection) == 0x4, "Field_MpdGeometrySection must keep its PS1 layout");

// decomp: _mpdHeader (src/BATTLE/BATTLE.PRG/146C.c:328)
struct Field_MpdHeader {
    int roomSectionOffset;
    u_short roomSectionSize;
    int clearedSectionOffset;
    u_short clearedSectionSize;
    int scriptSectionOffset;
    u_short scriptSectionSize;
    int doorSectionOffset;
    u_short doorSectionSize;
    int enemySectionOffset;
    int enemySectionSize;
    int lootSectionOffset;
    int lootSectionSize;
};
static_assert(sizeof(Field_MpdHeader) == 0x30, "Field_MpdHeader must keep its PS1 layout");

// decomp: _mpdPolygt3 (src/BATTLE/BATTLE.PRG/146C.h:44)
struct Field_MpdTriangle {
    short vertex1X, vertex1Y, vertex1Z;
    char vertex2OffsetX, vertex2OffsetY, vertex2OffsetZ;
    char vertex3OffsetX, vertex3OffsetY, vertex3OffsetZ;
    char red1, green1, blue1, polygonType;
    char red2, green2, blue2, u3;
    char red3, green3, blue3, v3;
    char u1, v1;
    short texelMask;
    char u2, v2;
    short textureId;
};
static_assert(sizeof(Field_MpdTriangle) == 0x20, "Field_MpdTriangle must keep its PS1 layout");

// decomp: _mpdPolygt4 (src/BATTLE/BATTLE.PRG/146C.h:59)
struct Field_MpdQuad {
    short vertex1X, vertex1Y, vertex1Z;
    char vertex2OffsetX, vertex2OffsetY, vertex2OffsetZ;
    char vertex3OffsetX, vertex3OffsetY, vertex3OffsetZ;
    char red1, green1, blue1, polygonType;
    char red2, green2, blue2, u3;
    char red3, green3, blue3, v3;
    char u1, v1;
    short texelMask;
    char u2, v2;
    short textureId;
    char vertex4OffsetX, vertex4OffsetY, vertex4OffsetZ, u4;
    char red4, green4, blue4, v4;
};
static_assert(sizeof(Field_MpdQuad) == 0x28, "Field_MpdQuad must keep its PS1 layout");

// decomp: _mpdRoomCollisionMapSection (src/BATTLE/BATTLE.PRG/146C.h:12)
struct Field_MpdCollisionHeader {
    short width;
    short height;
    short heightOrigin;
    short cellCount;
};
static_assert(sizeof(Field_MpdCollisionHeader) == 0x8, "Field_MpdCollisionHeader must keep its PS1 layout");

// decomp: _mpdRoomDoorSection_t (src/SLUS_010.40/main.h:446)
struct Field_MpdDoorRecord {
    u_char destinationZoneId;
    u_char destinationRoomId;
    Field_DoorTileLocation tileLocation;
    u_short requiredFlagValue : 8;
    u_short entryTileX : 5;
    u_short entryElevation : 2;
    u_short hasEntryStep : 1;
    u_short requiredFlagId : 10;
    u_short entryTileZ : 5;
    u_short reserved06Bit15 : 1;
    u_char doorId;
    u_char reserved09;
    u_char reserved0A;
    u_char reserved0B;
};
static_assert(sizeof(Field_MpdDoorRecord) == 0xC, "Field_MpdDoorRecord must keep its PS1 layout");

// decomp: _mpdRoomSection11 (src/BATTLE/BATTLE.PRG/146C.h:168)
struct Field_MpdViewTrigger {
    u_short tileX : 5;
    u_short tileZ : 5;
    u_short floorLayer : 1;
    u_short viewDirection : 3;
    u_short mode : 1;
    u_short reserved00Bit15 : 1;
    u_short effectCount;
    char effectIds[0];
};
static_assert(sizeof(Field_MpdViewTrigger) == 0x4, "Field_MpdViewTrigger must keep its PS1 layout");

// decomp: _mpdRoomSection3 (src/BATTLE/BATTLE.PRG/146C.h:81); upstream MpdTile
// (src/BATTLE/BATTLE.PRG/mpd.h:23). The "never read" bits were checked against
// every native port of the original's readers and all 92,049 cells on the disc
// (#347).
struct Field_MpdTileCell {
    u_int surfaceKind : 5;
    // Ramps and stairs: a rise between two slope cells is not a wall, and
    // stepping onto one slides instead of climbing (3A1A0.c:1731-1747).
    u_int isSlope : 1;
    u_int blocksSphereTerrain : 1;
    // Zero in every disc cell and never read.
    u_int padding00Bits7 : 3;
    // Upstream MpdTileShape: 0 open, 1-4 one triangular half closed, 5 blocked.
    u_int passability : 3;
    u_int hasEventTrigger : 1;
    u_int isPlatformDocked : 1;
    // Set in 544 disc cells, but nothing in the original reads it.
    u_int unusedBit15 : 1;
    u_int hasRoomTrigger : 1;
    u_int isPit : 1;
    // Bit 0 selects the camera occlusion; bit 1 keeps a blocked cell sampled as
    // floor by the target sphere (146C.c:5086, 58578.c:1014).
    u_int occlusionAndFloorFlags : 2;
    u_int forbidsGimmickPlacement : 1;
    // Zero in every disc cell and never read.
    u_int padding00Bits21 : 2;
    // The AI navigation map (func_800E6F9C) copies bits 23-31 into its tile
    // flags: the area (flags & 15), no-walk (0x10), no-flight (0x40) and three
    // terrain classes that actor reject masks test (0x2000, 0x1000, 0x800).
    u_int aiArea : 4;
    u_int aiNoWalk : 1;
    u_int aiNoFlight : 1;
    u_int aiTerrainClasses : 3;
};
static_assert(sizeof(Field_MpdTileCell) == 0x4, "Field_MpdTileCell must keep its PS1 layout");

// decomp: _mpdRoomSection12 (src/BATTLE/BATTLE.PRG/146C.h:193)
struct Field_MpdMovingPlatform {
    u_int sceneObjectId : 8;
    u_int isLooping : 1;
    u_int isHidden : 1;
    u_int endWaypoint : 2;
    u_int swapsColorsWhenFrozen : 2;
    u_int startWaypoint : 2;
    u_int reserved00Bits16 : 7;
    u_int reserved00Bits23 : 3;
    u_int isMovingForward : 1;
    u_int currentWaypoint : 2;
    u_int currentSegment : 3;
    u_int isWaiting : 1;
    u_int controlState : 7;
    u_int terminalWaypoint : 2;
    u_int reserved04Bits10 : 6;
    u_int motionTicks : 16;
    VECTOR waypoints[4];
    VECTOR segmentVelocities[6];
    int positionX;
    int positionY;
    int positionZ;
    Field_MpdTileCell tileCell;
};
static_assert(sizeof(Field_MpdMovingPlatform) == 0xB8, "Field_MpdMovingPlatform must keep its PS1 layout");

// decomp: _mpdRoomSection9 (src/BATTLE/BATTLE.PRG/146C.h:97)
struct Field_MpdFloorTrigger {
    short tileX;
    short tileZ;
    short floorLayer;
    short kind;
    union {
        short switchValue;
        struct {
            char state;
            char roomEffect;
        } lock;
    } lockState;
    char activation;
    char markerFade;
};

// decomp: _mpdRoomSectionA (src/BATTLE/BATTLE.PRG/146C.h:106)
struct Field_MpdLockTrigger {
    Field_MpdFloorTrigger trigger;
    short keyOrSigil;
    short isOpen;
    short linkedLockIndex;
    char isTriggered;
    char delayFrames;
};

// decomp: _mpdRoomSectionB (src/BATTLE/BATTLE.PRG/146C.h:136)
struct Field_MpdEnvironment {
    int backgroundColors[5];
    VECTOR backgroundRotation;
    int reserved24;
    int reserved28;
    int freezeEffectId;
    char isWorkshop;
    char environmentFlags;
    char trapLevel;
    char reserved33;
    short weatherKind;
    short windDirection;
    short windDirectionLimit;
    short windDirectionInterval;
    short weatherIntensity;
    short weatherIntensityLimit;
    short weatherIntensityInterval;
    short windStrength;
    short windStrengthLimit;
    short windStrengthInterval;
    union {
        short halves[2];
        int word;
    } weatherArea[2];
    short sceneId;
    short fallDestinationZone;
    short fallDestinationRoom;
    short opaque56;
};

// decomp: _mpdRoomSectionE (src/BATTLE/BATTLE.PRG/146C.h:145)
struct Field_MpdSectionE {
    int reserved00;
    int secondArrayOffset;
    int hasSecondArray;
    u_int firstArray; /* offset from the struct (on-disc pointer) */
    u_int secondArray; /* offset from the struct (on-disc pointer) */
    int payload[0];
};
static_assert(sizeof(Field_MpdSectionE) == 0x14, "Field_MpdSectionE must keep its PS1 layout");

// decomp: func_80091C6C_t2 (src/BATTLE/BATTLE.PRG/2842C.c:151)
struct Field_MpdPolygonUvView {
    char positionAndFirstColor[0xF];
    u_char type;
    char secondColor[3];
    u_char u2;
    char thirdColor[3];
    u_char v2;
    u_char u0;
    u_char v0;
    char texelMask[2];
    u_char u1;
    u_char v1;
    char textureIdAndFourthPosition[5];
    u_char u3;
    char fourthColor[3];
    u_char v3;
};
static_assert(sizeof(Field_MpdPolygonUvView) == 0x28, "Field_MpdPolygonUvView must keep its PS1 layout");

// decomp: func_80093914_t (src/BATTLE/BATTLE.PRG/2842C.c:98)
struct Field_MpdTriangleColorView {
    u_char positions[0xC];
    u_char red1;
    u_char green1;
    u_char blue1;
    u_char type;
    u_char red2;
    u_char green2;
    u_char blue2;
    u_char u3;
    u_char red3;
    u_char green3;
    u_char remainder[0xA];
};
static_assert(sizeof(Field_MpdTriangleColorView) == 0x20, "Field_MpdTriangleColorView must keep its PS1 layout");

// decomp: func_80093914_t2 (src/BATTLE/BATTLE.PRG/2842C.c:117)
struct Field_MpdQuadColorView {
    u_char positions[0xC];
    u_char red1;
    u_char green1;
    u_char blue1;
    u_char type;
    u_char red2;
    u_char green2;
    u_char blue2;
    u_char u3;
    u_char red3;
    u_char green3;
    u_char middle[0xE];
    u_char red4;
    u_char green4;
    u_char blue4;
    u_char v4;
};
static_assert(sizeof(Field_MpdQuadColorView) == 0x28, "Field_MpdQuadColorView must keep its PS1 layout");

// decomp: mpdObjectSpawn (src/BATTLE/BATTLE.PRG/146C.c:394)
struct Field_MpdObjectSpawn {
    u_char kind;
    u_char reserved01;
    u_char reserved02;
    u_char reserved03;
    u_short flag;
    u_char flagMin;
    u_char flagMax;
    u_char slot;
    u_char reserved09;
    u_char tileX;
    u_char tileZ;
    u_char height;
    u_char direction;
    u_short model;
    u_char reserved10[0x18];
};
static_assert(sizeof(Field_MpdObjectSpawn) == 0x28, "Field_MpdObjectSpawn must keep its PS1 layout");

#endif // VS_FIELD_MPD_H_TYPES
