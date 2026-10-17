// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_MAP_H_TYPES
#define VS_FIELD_MAP_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_MapRoom Field_MapRoom;
typedef struct Field_MapRoomName Field_MapRoomName;
typedef struct Field_MapRoomVertices Field_MapRoomVertices;
typedef struct Field_MapScene Field_MapScene;

// decomp: vs_battle_room (src/BATTLE/BATTLE.PRG/146C.h:152)
typedef struct Field_MapRoom {
    int visited;
    void* dataAddress;
    short zoneId;
    short mapId;
} Field_MapRoom;

// decomp: vs_battle_roomName (src/BATTLE/BATTLE.PRG/146C.h:646)
typedef struct Field_MapRoomName {
    char name[32];
    short prev;
    short next;
} Field_MapRoomName;

// decomp: vs_battle_roomVertices (src/BATTLE/BATTLE.PRG/146C.h:640)
typedef struct Field_MapRoomVertices {
    int vertexCount;
    SVECTOR vertices[0];
} Field_MapRoomVertices;

// decomp: vs_battle_scene (src/BATTLE/BATTLE.PRG/146C.h:157)
typedef struct Field_MapScene {
    int roomCount;
    Field_MapRoom rooms[0];
} Field_MapScene;

#endif // VS_FIELD_MAP_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_MAP_H_DECLS)
#define VS_FIELD_MAP_H_DECLS

int Field_SetMinimapVisible(char arg0);
void Field_RestoreSceneRoomVisits(Field_MapScene* scene);
void Field_BuildMinimapRotation(MATRIX* arg0, int arg1);
int Field_MapCompletionPercent(void);

#endif // VS_FIELD_MAP_H_DECLS
