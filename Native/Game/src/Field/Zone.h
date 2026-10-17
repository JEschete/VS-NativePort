// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ZONE_H_TYPES
#define VS_FIELD_ZONE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_DiscFile Content_DiscFile;
typedef struct Content_DiscQueueSlot Content_DiscQueueSlot;
typedef struct Field_MpdEnemySpawn Field_MpdEnemySpawn;
typedef struct Field_RoomLoadResetBlock Field_RoomLoadResetBlock;
typedef struct Field_ZndEnemyRecord Field_ZndEnemyRecord;
typedef struct Field_ZoneBounds Field_ZoneBounds;
typedef struct Field_ZoneContext Field_ZoneContext;

// decomp: D_800F18C8_t (src/BATTLE/BATTLE.PRG/146C.c:313)
typedef struct Field_RoomLoadResetBlock {
    short reserved00;
    short reserved02;
    short reserved04;
    short reserved06;
    short reserved08;
    short reserved0A;
    int reserved0C;
    int reserved10;
    int reserved14;
} Field_RoomLoadResetBlock;
_Static_assert(sizeof(Field_RoomLoadResetBlock) == 0x18, "Field_RoomLoadResetBlock must keep its PS1 layout");

// decomp: _zoneContextBounds (src/BATTLE/BATTLE.PRG/146C.h:214)
typedef struct Field_ZoneBounds {
    short minX;
    short minZ;
    short maxX;
    short maxZ;
} Field_ZoneBounds;
_Static_assert(sizeof(Field_ZoneBounds) == 0x8, "Field_ZoneBounds must keep its PS1 layout");

// decomp: _zoneContext_t (src/BATTLE/BATTLE.PRG/146C.h:719)
typedef struct Field_ZoneContext {
    int zndId;
    Content_DiscFile* mpdFiles;
    int zndEnemyCount;
    Field_ZndEnemyRecord* zndEnemies;
    Content_DiscFile* zudFiles;
    int loadedZudIndexPlusOne;
    Content_DiscQueueSlot* zudDiscRequest;
    void* zudData;
    Content_DiscQueueSlot* zndDiscRequest;
    void* zndData;
    int musicId;
    int musicSlot;
    int isMusicFadeEnabled;
    int mapId;
    int reserved38;
    short tileColumns;
    short tileRows;
    Field_ZoneBounds bounds;
    int reserved48;
    int reserved4C;
    int reserved50;
    int reserved54;
    Content_DiscQueueSlot* mpdDiscRequest;
    void* mpdData;
    u_int mpdEnemyCount;
    Field_MpdEnemySpawn* mpdEnemies;
    int currentSceneId;
} Field_ZoneContext;

#endif // VS_FIELD_ZONE_H_TYPES
