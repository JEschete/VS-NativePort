// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Boot/GameLoop.hpp"

#include <cstdint>

// decomp: vs_main_startState (vs_main_startState@SLUS_010.40)
std::int32_t VS::Boot::gameEntryMode;

// decomp: sp2 (sp2@SLUS_010.40)
std::int32_t VS::Boot::titleReturnStackPointer;

// decomp: _resetEnabled (_resetEnabled@SLUS_010.40)
std::int32_t VS::Boot::isSoftResetEnabled;

// decomp: sp (sp@SLUS_010.40)
std::int32_t VS::Boot::battleReturnStackPointer;

// decomp: vs_main_saveGameClearData (vs_main_saveGameClearData@SLUS_010.40)
std::int32_t VS::Boot::isClearSavePending;

// decomp: _inGame (_inGame@SLUS_010.40)
std::int32_t VS::Boot::isGameplayActive;
