// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/StatusEffects.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Score/ScoreScreen.h"

// decomp: D_800E8204 (D_800E8204@BATTLE/BATTLE.PRG)
int Battle_limbStatusFlags[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8490 (D_800E8490@BATTLE/BATTLE.PRG)
int Battle_vitalsTickCounter; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8494 (D_800E8494@BATTLE/BATTLE.PRG)
int Battle_regeneratingLimb; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8498 (D_800E8498@BATTLE/BATTLE.PRG)
int Battle_targetingRiskTimer; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_sceneBuffer (vs_battle_sceneBuffer@BATTLE/BATTLE.PRG)
void* Battle_unusedSceneBuffer;

// decomp: vs_battle_menu9CursorMemory (vs_battle_menu9CursorMemory@BATTLE/BATTLE.PRG)
Menu_Score_CursorMemory Battle_unusedMenu9CursorMemory;

// decomp: vs_battle_submenuState (vs_battle_submenuState@BATTLE/BATTLE.PRG)
int Battle_unusedSubmenuState;

// decomp: D_800F4590 (D_800F4590@BATTLE/BATTLE.PRG)
Menu_Framework_PreviewPoseState* Battle_unusedGlobal1[1];

// decomp: D_800F4594 (D_800F4594@BATTLE/BATTLE.PRG)
int Battle_unusedGlobal2;

// decomp: vs_main_statusEffectParams (vs_main_statusEffectParams@SLUS_010.40)
Battle_StatusEffectParams Battle_statusEffectParams[32]; /* contents: module image (ModuleData.cpp) */

}  // extern "C"
