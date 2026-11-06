// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Boot/GameLoop.h"

// decomp: vs_main_startState (vs_main_startState@SLUS_010.40)
int Boot_gameEntryMode;

// decomp: sp2 (sp2@SLUS_010.40)
int Boot_titleReturnStackPointer;

// decomp: _resetEnabled (_resetEnabled@SLUS_010.40)
int Boot_isSoftResetEnabled;

// decomp: sp (sp@SLUS_010.40)
int Boot_battleReturnStackPointer;

// decomp: vs_main_saveGameClearData (vs_main_saveGameClearData@SLUS_010.40)
int Boot_isClearSavePending;

// decomp: _inGame (_inGame@SLUS_010.40)
int Boot_isGameplayActive;

}  // extern "C"
