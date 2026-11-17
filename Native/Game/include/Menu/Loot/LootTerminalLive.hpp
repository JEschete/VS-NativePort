#pragma once

#include <cstdint>

#include "Menu/Loot/LootTerminal.hpp"

namespace VS::Menu::Loot {
int UpdateLiveInventoryTransition(int initialize_category);
int UpdateLiveDisassembly(int initialize_row);
int UpdateLiveStackConsolidation(int initialize_row);
int UpdateLiveLootSelection(int initialize);
int UpdateLiveLootScreen(unsigned char* step);

// Restores file-backed controllers without releasing the owned workspace.
void ResetLootTerminalModuleData();
// Cold-session reset releases native-owned storage and invalidates its
// borrowed globals; never use this as the ordinary overlay reload hook.
void ResetLootTerminalSession();
[[nodiscard]] std::uint64_t LootTerminalLiveRefusalCount() noexcept;
}  // namespace VS::Menu::Loot
