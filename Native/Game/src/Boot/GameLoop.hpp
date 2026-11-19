#pragma once
// The game loop's entry mode, soft-reset state and saved stack positions.
#include <cstdint>

namespace VS::Boot {
// decomp: vs_main_startState (vs_main_startState@SLUS_010.40)
extern std::int32_t gameEntryMode;
// decomp: sp2 (sp2@SLUS_010.40)
extern std::int32_t titleReturnStackPointer;
// decomp: _resetEnabled (_resetEnabled@SLUS_010.40)
extern std::int32_t isSoftResetEnabled;
// decomp: sp (sp@SLUS_010.40)
extern std::int32_t battleReturnStackPointer;
// decomp: vs_main_saveGameClearData (vs_main_saveGameClearData@SLUS_010.40)
extern std::int32_t isClearSavePending;
// decomp: _inGame (_inGame@SLUS_010.40)
extern std::int32_t isGameplayActive;
}  // namespace VS::Boot
