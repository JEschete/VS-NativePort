// The battle HUD's state and sprite tables (Hud.hpp).
#include "Battle/Hud.hpp"

namespace VS::Battle {
// decomp: vs_battle_limbNames (vs_battle_limbNames@BATTLE/BATTLE.PRG);
// contents: module image, relocated (ModulePointers.cpp)
std::array<char*, kLimbNameCount> limbNames;
// The globals below hold module image contents (ModuleData.cpp) unless noted.
// decomp: D_800EB9AE (D_800EB9AE@BATTLE/BATTLE.PRG)
std::int8_t hudStatusIconsHidden;
// decomp: D_800EB9AF (D_800EB9AF@BATTLE/BATTLE.PRG)
std::int8_t hudHidden;
// decomp: D_800EB9B0 (D_800EB9B0@BATTLE/BATTLE.PRG)
std::int32_t hudSlideOffset;
// decomp: D_800EB9CC (D_800EB9CC@BATTLE/BATTLE.PRG)
std::int8_t hudBattleMode;
// decomp: D_800EB9CD (D_800EB9CD@BATTLE/BATTLE.PRG)
std::int8_t battleModeIndicatorStep;
// decomp: D_800EB9D0 (D_800EB9D0@BATTLE/BATTLE.PRG); module symbol
// (ModulePointers.cpp)
StatusIconCarousel statusIconCarousel;
// decomp: D_800EBBDC (D_800EBBDC@BATTLE/BATTLE.PRG)
std::array<std::int32_t, kHudGaugeCount> hudGaugePositions;
// decomp: D_800EBBEC (D_800EBBEC@BATTLE/BATTLE.PRG)
std::array<std::int32_t, kHudGaugeCount> hudGaugeLabelPositions;
// decomp: D_800EBBFC (D_800EBBFC@BATTLE/BATTLE.PRG)
std::array<std::int8_t, kHudGaugeCount> hudGaugeLabelWidths;
// decomp: D_800EBC00 (D_800EBC00@BATTLE/BATTLE.PRG)
std::array<std::int8_t, kHudGaugeLabelTexUBytes> hudGaugeLabelTexU;
// decomp: vs_battle_statusIconTexOffsets
// (vs_battle_statusIconTexOffsets@BATTLE/BATTLE.PRG)
StatusIconTexOffsetTable statusIconTexOffsets;
// decomp: D_800EBC54 (D_800EBC54@BATTLE/BATTLE.PRG)
std::array<std::uint32_t, kLimbStatusColorCount> limbStatusColors;
// decomp: D_800EBC68 (D_800EBC68@BATTLE/BATTLE.PRG)
UpperStatusIconFlagTable upperStatusIconFlags;
// decomp: D_800EBC78 (D_800EBC78@BATTLE/BATTLE.PRG)
std::int8_t shortcutHudRequested;
// decomp: D_800EBC88 (D_800EBC88@BATTLE/BATTLE.PRG)
FirstShortcutDigitOffsetTable firstShortcutDigitOffsets;
// decomp: D_800EBC98 (D_800EBC98@BATTLE/BATTLE.PRG)
SecondShortcutMarkerPositionTable secondShortcutMarkerPositions;
// decomp: D_800EBCA0 (D_800EBCA0@BATTLE/BATTLE.PRG)
SecondShortcutIconPositionTable secondShortcutIconPositions;
// decomp: D_800EBCA8 (D_800EBCA8@BATTLE/BATTLE.PRG)
SecondShortcutIconSizeTable secondShortcutIconSizes;
// decomp: D_800EBCB8 (D_800EBCB8@BATTLE/BATTLE.PRG)
SecondShortcutIconTexCoordTable secondShortcutIconTexCoords;
// decomp: D_800EBCC0 (D_800EBCC0@BATTLE/BATTLE.PRG)
FirstShortcutMarkerPositionTable firstShortcutMarkerPositions;
// decomp: D_800EBCC8 (D_800EBCC8@BATTLE/BATTLE.PRG)
FirstShortcutIconPositionTable firstShortcutIconPositions;
// decomp: D_800EBCD0 (D_800EBCD0@BATTLE/BATTLE.PRG)
FirstShortcutIconSizeTable firstShortcutIconSizes;
// decomp: D_800EBCE0 (D_800EBCE0@BATTLE/BATTLE.PRG)
FirstShortcutIconTexCoordTable firstShortcutIconTexCoords;
// decomp: D_800EBCE8 (D_800EBCE8@BATTLE/BATTLE.PRG)
std::uint8_t shortcutHudAnimationStep;
// decomp: D_800EBCEC (D_800EBCEC@BATTLE/BATTLE.PRG)
LimbDisplaySpriteOffsetTable limbDisplaySpriteOffsets;
// decomp: D_800EBD14 (D_800EBD14@BATTLE/BATTLE.PRG)
LimbDisplaySpriteSizeTable limbDisplaySpriteSizes;
// decomp: D_800EBD3C (D_800EBD3C@BATTLE/BATTLE.PRG)
LimbDisplaySpriteTexCoordTable limbDisplaySpriteTexCoords;
// decomp: D_800EBD64 (D_800EBD64@BATTLE/BATTLE.PRG)
std::uint16_t battleModeIndicatorWobble;
// decomp: D_800EC258 (D_800EC258@BATTLE/BATTLE.PRG); module symbol
// (ModulePointers.cpp)
char* puzzleTimerTextPointer;
// decomp: vs_battle_lowerScreenUiState
// (vs_battle_lowerScreenUiState@BATTLE/BATTLE.PRG)
std::int8_t lowerScreenUiState;
// decomp: D_80069998 (D_80069998@BATTLE/BATTLE.PRG)
std::array<char, kPuzzleTimerTextBytes> puzzleTimerText = {"00:00:00"};
}  // namespace VS::Battle
