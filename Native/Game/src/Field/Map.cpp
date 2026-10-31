// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Field/Map.h"
#include "Field/MapProgress.h"
#include "Field/Minimap.h"
#include "Field/RoomState.h"
#include "State/Progress.h"

// decomp: BATTLE.PRG func_8008B4C8 (src/BATTLE/BATTLE.PRG/146C.c:16012)
int Field_SetMinimapVisible(char visible) { return Field_isMinimapVisible = visible; }

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
void Field_BuildMinimapRotation(MATRIX* output_rotation, int yaw_angle)
{
    int yaw_sine = Core_Gte_Sin(yaw_angle);
    int yaw_cosine = Core_Gte_Cos(yaw_angle);
    int negative_yaw_sine = -yaw_sine;

    output_rotation->m[0][0] = yaw_cosine;
    output_rotation->m[0][1] = 0;
    output_rotation->m[0][2] = yaw_sine;

    output_rotation->m[1][0] = ((negative_yaw_sine * -3138) / ONE);
    output_rotation->m[1][1] = 2633;
    output_rotation->m[1][2] = (-(yaw_cosine * 3138) / ONE);

    output_rotation->m[2][0] = ((negative_yaw_sine * 2633) / ONE);
    output_rotation->m[2][1] = 3138;
    output_rotation->m[2][2] = ((yaw_cosine * 2633) / ONE);
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

}  // extern "C"
