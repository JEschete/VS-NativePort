// The BATTLE.PRG menu tables; ModuleData.cpp fills them from the module image.
#include "Menu/Framework/Menus.hpp"

namespace VS::Menu {
// decomp: vs_battle_menuStrings (vs_battle_menuStrings@BATTLE/BATTLE.PRG)
std::uint16_t menuStrings[kBattleMenuStringHalfwords];
}  // namespace VS::Menu

namespace VS::Menu::Framework {
// decomp: D_800EBBB8 (D_800EBBB8@BATTLE/BATTLE.PRG)
std::uint32_t gradientTopColors[kGradientTopColorWords];
// decomp: D_800EBBC0 (D_800EBBC0@BATTLE/BATTLE.PRG)
std::uint32_t gradientBottomColors[kGradientBottomColorWords];
// decomp: vs_battle_rowAnimationSteps
// (vs_battle_rowAnimationSteps@BATTLE/BATTLE.PRG)
std::uint8_t rowAnimationSteps[kRowAnimationStepCount];
// decomp: vs_battle_animationIndices
// (vs_battle_animationIndices@BATTLE/BATTLE.PRG)
std::uint8_t animationIndices[kAnimationIndexCount];
// decomp: D_800EBD68 (D_800EBD68@BATTLE/BATTLE.PRG)
std::uint8_t abilityGroupActionRanges[kAbilityGroupActionRangeBytes];
// decomp: vs_battle_cursorBrightnessAnimation
// (vs_battle_cursorBrightnessAnimation@BATTLE/BATTLE.PRG)
std::uint8_t cursorBrightnessAnimation[kCursorBrightnessBytes];
}  // namespace VS::Menu::Framework
