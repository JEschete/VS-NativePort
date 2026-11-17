#pragma once

#include <cstdint>

namespace VS::Menu::BattleAbilities {  // NOLINT(readability-identifier-naming)
void LiveToggleButtonAssignment(int type, int button, int ability_index);
int LiveUpdateAssignments();
int LiveUpdateChainAbilityMenu(int initialize);
int LiveUpdateDefenseAbilityMenu(int initialize);
void LiveDrawNextUnlockPoints(int x_offset);
int LiveUpdateAbilityScreen(std::uint8_t* phase);

// Reload and cold reset restore this component's image-backed statics,
// including learned lists and animation. MENU2's six-sector read covers each.
// The outer menu/heap owner still owns shared row state and its allocation.
void ResetLiveAbilityTerminalModuleState();
void ResetLiveAbilityTerminalColdState();
}  // namespace VS::Menu::BattleAbilities
