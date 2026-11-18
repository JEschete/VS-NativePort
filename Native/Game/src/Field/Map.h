// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Field_MapRoom;
struct Field_MapRoomName;
struct Field_MapRoomVertices;
struct Field_MapScene;

// decomp: vs_battle_room (src/BATTLE/BATTLE.PRG/146C.h:152)
struct Field_MapRoom {
    int visited;
    u_int dataAddress; /* offset from the struct (on-disc pointer) */
    short zoneId;
    short mapId;
};
static_assert(sizeof(Field_MapRoom) == 0xC, "Field_MapRoom must keep its PS1 layout");

// decomp: vs_battle_roomName (src/BATTLE/BATTLE.PRG/146C.h:646)
struct Field_MapRoomName {
    char name[32];
    short prev;
    short next;
};
static_assert(sizeof(Field_MapRoomName) == 0x24, "Field_MapRoomName must keep its PS1 layout");

// decomp: vs_battle_roomVertices (src/BATTLE/BATTLE.PRG/146C.h:640)
struct Field_MapRoomVertices {
    int vertexCount;
    SVECTOR vertices[0];
};
static_assert(sizeof(Field_MapRoomVertices) == 0x4, "Field_MapRoomVertices must keep its PS1 layout");

// decomp: vs_battle_scene (src/BATTLE/BATTLE.PRG/146C.h:157)
struct Field_MapScene {
    int roomCount;
    Field_MapRoom rooms[0];
};
static_assert(sizeof(Field_MapScene) == 0x4, "Field_MapScene must keep its PS1 layout");
