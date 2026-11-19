#pragma once
// The BATTLE.PRG menu tables (5BF94.c) the menus read, loaded from the module
// image (ModuleData.cpp). The byte tables are unsigned: the original builds
// with -funsigned-char and reads them with lbu.
#include <cstddef>
#include <cstdint>

namespace VS::Menu {
inline constexpr std::size_t kBattleMenuStringHalfwords = 246;

// decomp: vs_battle_menuStrings (vs_battle_menuStrings@BATTLE/BATTLE.PRG):
// halfword offsets to the strings, then the encoded strings.
extern std::uint16_t menuStrings[kBattleMenuStringHalfwords];
}  // namespace VS::Menu

namespace VS::Menu::Framework {
inline constexpr std::size_t kGradientTopColorWords = 4;
inline constexpr std::size_t kGradientBottomColorWords = 2;
inline constexpr std::size_t kRowAnimationStepCount = 36;
inline constexpr std::size_t kAnimationIndexCount = 244;
inline constexpr std::size_t kAbilityGroupActionRangeBytes = 8;
inline constexpr std::size_t kCursorBrightnessBytes = 22012;

// decomp: D_800EBBB8 (D_800EBBB8@BATTLE/BATTLE.PRG): the top gradient color of
// menu row colors 16 and up.
extern std::uint32_t gradientTopColors[kGradientTopColorWords];
// decomp: D_800EBBC0 (D_800EBBC0@BATTLE/BATTLE.PRG): the bottom gradient color
// of menu row colors 16 and up.
extern std::uint32_t gradientBottomColors[kGradientBottomColorWords];
// decomp: vs_battle_rowAnimationSteps
// (vs_battle_rowAnimationSteps@BATTLE/BATTLE.PRG): the x offset of each menu
// row slide step.
extern std::uint8_t rowAnimationSteps[kRowAnimationStepCount];
// decomp: vs_battle_animationIndices
// (vs_battle_animationIndices@BATTLE/BATTLE.PRG): the next step of each menu
// slide animation.
extern std::uint8_t animationIndices[kAnimationIndexCount];
// decomp: D_800EBD68 (D_800EBD68@BATTLE/BATTLE.PRG): the action id range of
// three ability groups; a group's menu flag is set once any of its actions is
// learned.
extern std::uint8_t abilityGroupActionRanges[kAbilityGroupActionRangeBytes];
// decomp: vs_battle_cursorBrightnessAnimation
// (vs_battle_cursorBrightnessAnimation@BATTLE/BATTLE.PRG): the brightness of
// each cursor blink step.
extern std::uint8_t cursorBrightnessAnimation[kCursorBrightnessBytes];
}  // namespace VS::Menu::Framework
