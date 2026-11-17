#pragma once

#include <cstdint>
#include <span>

namespace VS::Menu::Inventory {
// MENUB.PRG's 644-byte menuText asset (module-relative +0x7A80).
// The accessor loads it on first use after a cold reset unless already bound.
// The bytes are checked and copied, not borrowed.
void BindLiveLootInventoryText(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::span<const std::uint8_t> LiveLootInventoryText();
// MENU3.PRG (_ITEMMENU) and MENUB.PRG have independent static controllers.
int UpdateLiveSortMenu(int initialize_category);
int UpdateLiveLootSortMenu(int initialize_category);
int UpdateLiveDiscardMenu(int packed_entry);
int UpdateLiveLootDiscardMenu(int packed_entry);
int UpdateLiveItemList(int initialize);
int UpdateLiveLootItemList(int initialize);
void UpdateLiveMiscItemShortcut(int initialize);
int UpdateLiveInventoryScreen(unsigned char* step);

// Overlay reloads reset initialized data only, preserving original BSS.
void ResetInventoryTerminalModuleData();
void ResetLootInventoryTerminalModuleData();
// Explicit cold-session/test reset, not an ordinary overlay-load hook.
void ResetInventoryTerminalSession();
[[nodiscard]] std::uint64_t InventoryTerminalLiveRefusalCount() noexcept;
}  // namespace VS::Menu::Inventory
