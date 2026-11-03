#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Items_Inventory;

// Binds the inventory rules (Inventory.hpp) to Ashley's inventory
// (Audio_Seq_unusedInventory), its menu display order
// (Audio_Seq_unusedInventoryIndices) and her battle actor, in the shapes the
// copied callers use. Categories are the menu categories 0-7; another is
// refused and reported.
namespace VS::Items {
// How many of item `id` Ashley holds.
int CountLiveInventoryItem(int id);
// Takes one of misc item `misc_id`; 1 when she held one.
int ConsumeLiveMiscItem(int misc_id);
}  // namespace VS::Items

namespace VS::Items::Inventory {
void UnequipLiveWeapon();
void UnequipLiveShield();
// Empties 1-based slot `slot` of the category.
void ClearLiveItemSlot(int category, int slot);
// The 1-based display position of 0-based slot `slot`, or 0.
int FindLiveDisplayPosition(int category, int slot);
void RebuildLiveDisplayIndices(int category);
// A null inventory is Ashley's.
int CountLiveItems(int category, const Items_Inventory* inventory);
int FindLiveFirstEmptySlot(int category, const Items_Inventory* inventory);
// Copies 0-based slot `source_index` of `source` into `target` (CopyItem's
// flags); the 1-based slot it took or would take, or 0.
int CopyLiveItem(int flags, Items_Inventory* target, int source_index,
                 const Items_Inventory* source);
void StoreLiveEquippedStats();
}  // namespace VS::Items::Inventory
