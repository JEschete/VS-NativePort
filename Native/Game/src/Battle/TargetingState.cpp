// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/TargetingState.h"
#include "Menu/Framework/TextBoxes.h"

// decomp: D_800E8184 (D_800E8184@BATTLE/BATTLE.PRG)
char Battle_categoryNeedsTargetSelection[96]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8194 (D_800E8194@BATTLE/BATTLE.PRG)
u_char Battle_categoryIsAreaTarget[80]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E81B4 (D_800E81B4@BATTLE/BATTLE.PRG)
u_char Battle_categoryIncludesActingActor[48]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E81C4 (D_800E81C4@BATTLE/BATTLE.PRG)
u_char Battle_categoryTargetsBodyParts[32]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E81D4 (D_800E81D4@BATTLE/BATTLE.PRG)
u_char Battle_categoryTargetActorMasks[16]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_statusStrings (vs_battle_statusStrings@BATTLE/BATTLE.PRG)
u_short Battle_statusStrings[1712]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA984 (D_800EA984@BATTLE/BATTLE.PRG)
char Battle_targetInfoTemplate[3140]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EA9B6 (D_800EA9B6@BATTLE/BATTLE.PRG)
char Battle_targetStatusLineTemplate[3090]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EAA54 (D_800EAA54@BATTLE/BATTLE.PRG)
char Battle_emptyStatusString[2932]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EABAA (D_800EABAA@BATTLE/BATTLE.PRG)
u_char Battle_targetSelectionStep; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4C70 (D_800F4C70@BATTLE/BATTLE.PRG)
u_char Battle_targetColumnEntries[2][16];

// decomp: D_800F4C90 (D_800F4C90@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnGroups[8];

// decomp: D_800F4C98 (D_800F4C98@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnGroups[8];

// decomp: D_800F4CA0 (D_800F4CA0@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnGroupFirstTargets[8];

// decomp: D_800F4CA8 (D_800F4CA8@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnGroupFirstTargets[8];

// decomp: D_800F4CB0 (D_800F4CB0@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnEntryCount;

// decomp: D_800F4CB1 (D_800F4CB1@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnEntryCount;

// decomp: D_800F4CB2 (D_800F4CB2@BATTLE/BATTLE.PRG)
u_char Battle_secondColumnGroupCount;

// decomp: D_800F4CB3 (D_800F4CB3@BATTLE/BATTLE.PRG)
u_char Battle_firstColumnGroupCount;

// decomp: D_800F4CB4 (D_800F4CB4@BATTLE/BATTLE.PRG)
u_char Battle_targetColumnTopOffset;

// decomp: vs_battle_stringContext (vs_battle_stringContext@BATTLE/BATTLE.PRG)
Menu_StringContext Battle_stringContext;

}  // extern "C"
