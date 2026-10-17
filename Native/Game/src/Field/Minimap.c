// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Field/Map.h"
#include "Field/Minimap.h"

// decomp: D_800F1BA0 (D_800F1BA0@BATTLE/BATTLE.PRG)
int Field_minimapBorderPulsePhase;

// decomp: D_800F1CD4 (D_800F1CD4@BATTLE/BATTLE.PRG)
short Field_minimapSlideFrame;

// decomp: D_800F1CD6 (D_800F1CD6@BATTLE/BATTLE.PRG)
char Field_isMinimapVisible;

// decomp: BATTLE.PRG func_80098F8C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Field_DrawMinimapScene(void*)
{
    VS_UNPORTED("Field::DrawMinimapScene");
}

// decomp: BATTLE.PRG vs_battle_initSceneAndGetRoomNames (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
Field_MapRoomName* Field_ResolveSceneRoomData(void*)
{
    VS_UNPORTED("Field::ResolveSceneRoomData");
    return 0;
}

