// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Hud.h"

// decomp: vs_battle_limbNames (vs_battle_limbNames@BATTLE/BATTLE.PRG)
char* Battle_limbNames[20]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EB9AE (D_800EB9AE@BATTLE/BATTLE.PRG)
char Battle_hudStatusIconsHidden; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9AF (D_800EB9AF@BATTLE/BATTLE.PRG)
char Battle_hudHidden; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9B0 (D_800EB9B0@BATTLE/BATTLE.PRG)
int Battle_hudSlideOffset; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9CC (D_800EB9CC@BATTLE/BATTLE.PRG)
char Battle_hudBattleMode; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9CD (D_800EB9CD@BATTLE/BATTLE.PRG)
char Battle_battleModeIndicatorStep; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9D0 (D_800EB9D0@BATTLE/BATTLE.PRG)
union Battle_HudHighlightWord Battle_statusIconCarousel; /* data: contents loaded in Step 5 */

// decomp: D_800EBBDC (D_800EBBDC@BATTLE/BATTLE.PRG)
int Battle_hudGaugePositions[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBBEC (D_800EBBEC@BATTLE/BATTLE.PRG)
int Battle_hudGaugeLabelPositions[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBBFC (D_800EBBFC@BATTLE/BATTLE.PRG)
char Battle_hudGaugeLabelWidths[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC00 (D_800EBC00@BATTLE/BATTLE.PRG)
char Battle_hudGaugeLabelTexU[20]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_statusIconTexOffsets (vs_battle_statusIconTexOffsets@BATTLE/BATTLE.PRG)
u_short Battle_statusIconTexOffsets[32]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC54 (D_800EBC54@BATTLE/BATTLE.PRG)
u_int Battle_limbStatusColors[10]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC68 (D_800EBC68@BATTLE/BATTLE.PRG)
u_short Battle_upperStatusIconFlags[10]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC78 (D_800EBC78@BATTLE/BATTLE.PRG)
char Battle_shortcutHudRequested; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC88 (D_800EBC88@BATTLE/BATTLE.PRG)
int Battle_firstShortcutDigitOffsets[58]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC98 (D_800EBC98@BATTLE/BATTLE.PRG)
u_char Battle_secondShortcutMarkerPositions[216]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCA0 (D_800EBCA0@BATTLE/BATTLE.PRG)
u_char Battle_secondShortcutIconPositions[208]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCA8 (D_800EBCA8@BATTLE/BATTLE.PRG)
int Battle_secondShortcutIconSizes[50]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCB8 (D_800EBCB8@BATTLE/BATTLE.PRG)
u_short Battle_secondShortcutIconTexCoords[92]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCC0 (D_800EBCC0@BATTLE/BATTLE.PRG)
u_char Battle_firstShortcutMarkerPositions[176]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCC8 (D_800EBCC8@BATTLE/BATTLE.PRG)
u_char Battle_firstShortcutIconPositions[168]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCD0 (D_800EBCD0@BATTLE/BATTLE.PRG)
int Battle_firstShortcutIconSizes[40]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCE0 (D_800EBCE0@BATTLE/BATTLE.PRG)
u_short Battle_firstShortcutIconTexCoords[72]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCE8 (D_800EBCE8@BATTLE/BATTLE.PRG)
u_char Battle_shortcutHudAnimationStep; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCEC (D_800EBCEC@BATTLE/BATTLE.PRG)
int Battle_limbDisplaySpriteOffsets[33]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBD14 (D_800EBD14@BATTLE/BATTLE.PRG)
int Battle_limbDisplaySpriteSizes[23]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBD3C (D_800EBD3C@BATTLE/BATTLE.PRG)
u_int Battle_limbDisplaySpriteTexCoords[13]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBD64 (D_800EBD64@BATTLE/BATTLE.PRG)
u_short Battle_battleModeIndicatorWobble; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC258 (D_800EC258@BATTLE/BATTLE.PRG)
char* Battle_puzzleTimerTextPointer; /* data: contents loaded in Step 5 */

// decomp: vs_battle_lowerScreenUiState (vs_battle_lowerScreenUiState@BATTLE/BATTLE.PRG)
char Battle_lowerScreenUiState;

// decomp: D_80069998 (D_80069998@BATTLE/BATTLE.PRG)
char Battle_puzzleTimerText[9] = "00:00:00";

// decomp: D_8005046C (D_8005046C@SLUS_010.40)
int Battle_puzzleTimer;

}  // extern "C"
