// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Field/Map.h"
#include "Field/MapProgress.h"
#include "Field/Minimap.h"
#include "Field/RoomState.h"
#include "State/Progress.h"

// decomp: BATTLE.PRG func_8008B4C8 (src/BATTLE/BATTLE.PRG/146C.c:16012)
int Field_SetMinimapVisible(char arg0) { return Field_isMinimapVisible = arg0; }

// decomp: BATTLE.PRG vs_battle_setRoomsUnk0 (src/BATTLE/BATTLE.PRG/146C.c:16014)
void Field_RestoreSceneRoomVisits(Field_MapScene* scene)
{
    int i;
    Field_MapRoom* room = scene->rooms;
    int roomCount = scene->roomCount;

    for (i = 0; i < roomCount; ++i, ++room) {
        int locationId = Field_FindLocationId(room->zoneId, room->mapId - 1);
        if (locationId > 0) {
            room->visited =
                State_roomVisitProgress.roomFlags[locationId >> 5] & (1 << (locationId & 0x1F));
        } else {
            room->visited = 0;
        }
    }
}

// decomp: BATTLE.PRG func_8008B590 (src/BATTLE/BATTLE.PRG/146C.c:16033)
void Field_BuildMinimapRotation(MATRIX* arg0, int arg1)
{
    int temp_s0 = Core_Gte_Sin(arg1);
    int temp_v0 = Core_Gte_Cos(arg1);
    int a2 = -temp_s0;

    arg0->m[0][0] = temp_v0;
    arg0->m[0][1] = 0;
    arg0->m[0][2] = temp_s0;

    arg0->m[1][0] = ((a2 * -3138) / ONE);
    arg0->m[1][1] = 2633;
    arg0->m[1][2] = (-(temp_v0 * 3138) / ONE);

    arg0->m[2][0] = ((a2 * 2633) / ONE);
    arg0->m[2][1] = 3138;
    arg0->m[2][2] = ((temp_v0 * 2633) / ONE);
}

// decomp: BATTLE.PRG vs_battle_getMapCompletion (src/BATTLE/BATTLE.PRG/146C.c:17367)
int Field_MapCompletionPercent(void)
{
    int i;
    int j;
    int value;

    if (State_roomVisitProgress.roomFlags[1] & 0x800000) {
        State_roomVisitProgress.roomFlags[1] |= 0x400000;
    }
    value = 0;
    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 32; ++j) {
            int v = 1;
            if (State_roomVisitProgress.roomFlags[i] & Field_countableRoomMask[i]
                & (v << j)) {
                ++value;
            }
        }
    }
    return (value * 100) / 361;
}

