// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/RoomState.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Battle/ValueLabels.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Field/Background.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Locations.h"
#include "Field/Manual.h"
#include "Field/Map.h"
#include "Field/Minimap.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/RoomState.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/Score/ScoreScreen.h"
#include "State/Progress.h"

// decomp: D_800F19A0 (D_800F19A0@BATTLE/BATTLE.PRG)
int Field_isRoomVisitPending;

// decomp: BATTLE.PRG _setRoomSeen (src/BATTLE/BATTLE.PRG/146C.c:3205)
void Field_MarkCurrentRoomVisited(void)
{
    u_int scene;
    u_int room;

    if (Field_isRoomVisitPending == 0) {
        return;
    }

    scene = Field_CurrentSceneId();
    State_roomVisitProgress.areaFlags[scene >> 5] |= 1 << (scene & 0x1F);
    room = Field_CurrentRoomId();
    Field_isRoomVisitPending = 0;
    State_roomVisitProgress.roomFlags[room >> 5] |= 1 << (room & 0x1F);
}

// decomp: BATTLE.PRG func_8007BBB8 (src/BATTLE/BATTLE.PRG/146C.c:8509)
void Field_ClearSavedRoomEnemies(int arg0, int arg1) { Actors_ClearCachedRoomActorStates(arg0, arg1); }

// decomp: BATTLE.PRG func_8007BF2C (src/BATTLE/BATTLE.PRG/146C.c:8619)
void Field_ShowPrimaryRoomGeometry(void) { Field_EnableOwnedRoomRendering(0); }

// decomp: BATTLE.PRG func_8007BF4C (src/BATTLE/BATTLE.PRG/146C.c:8621)
void Field_HidePrimaryRoomGeometry(void) { Field_DisableOwnedRoomRendering(0); }

// decomp: BATTLE.PRG func_8007C4D4 (src/BATTLE/BATTLE.PRG/146C.c:8803)
void Field_CancelPendingRoomVisit(void) { Field_isRoomVisitPending = 0; }

// decomp: BATTLE.PRG _getLocationId (src/BATTLE/BATTLE.PRG/146C.c:8899)
int Field_FindLocationId(int zoneId, int mapId)
{
    int i;

    for (i = 0; i < 512; ++i) {
        if (zoneId == Field_locationDestinations[i * 2]
            && mapId == Field_locationDestinations[i * 2 + 1]) {
            return i;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_getCurrentRoomId (src/BATTLE/BATTLE.PRG/146C.c:8912)
int Field_CurrentRoomId(void)
{
    return Field_FindLocationId(Field_zoneContext.zndId, Field_zoneContext.mapId);
}

// decomp: INITBTL.PRG func_800FA6B8 (src/BATTLE/INITBTL.PRG/18.c:595)
void Field_InitializeRoomRuntimeState(void)
{
    Core_Memory_ZeroAlignedWords(Field_backgroundReferenceVectors, sizeof Field_backgroundReferenceVectors);
    Core_Memory_ZeroAlignedWords(&Field_roomData, sizeof Field_roomData);
    Core_Memory_ZeroAlignedWords(&Field_entranceDoorRecord, sizeof Field_entranceDoorRecord);
    Core_Memory_ZeroAlignedWords(&Battle_unusedMenu9CursorMemory, sizeof Battle_unusedMenu9CursorMemory);
    Core_Memory_ZeroAlignedWords(&Field_manualDisplayState, sizeof Field_manualDisplayState);
    Field_roomPalettes = Core_Memory_AllocateFromLowEnd(0x800);
    Battle_floatingLabels = Core_Memory_AllocateFromLowEnd(sizeof *Battle_floatingLabels);
    Battle_placedLabelPositions = Core_Memory_AllocateFromLowEnd(0x80);
    Field_enteredDoorId = -1;
    Field_areRoomUpdatesPaused = 0;
    Field_isRoomTintActive = 0;
    Battle_unusedSubmenuState = 0;
    Battle_unusedSceneBuffer = NULL;
    Field_isMinimapVisible = 1;
    Field_minimapSlideFrame = 8;
    Field_InitializeRoomBackground();
    Field_ResetGimmickAnimationQueue();
    Field_SetRoomBackgroundColors((int*)Field_backgroundReferenceVectors);
}

