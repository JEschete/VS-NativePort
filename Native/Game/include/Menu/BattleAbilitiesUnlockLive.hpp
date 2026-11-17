#pragma once

#include <cstdint>
#include <span>

namespace VS::Menu::BattleAbilities {  // NOLINT(readability-identifier-naming)
int LiveUpdateBreakArtUnlock(int initialize);
[[nodiscard]] char* LiveUnlockDescription(int ability);
int LiveUpdateAbilityUnlock(int initialize);
int LiveUpdateUnlockNotifications(char* state);
void LiveSetRiskCost(int action);
int LiveCreateAbilityRows(int count, int type,
                          std::span<std::uint16_t* const> menu_text);
// Original 0x80102928 entry: callers supply two text pointers per row.
int LiveCreateAbilityRowsEntry(int count, int type, std::uint16_t** menu_text);

// MENU2 reload resets these transient values; the menu/heap owner frees rows.
void ResetLiveAbilityFoundationState();
}  // namespace VS::Menu::BattleAbilities
