#pragma once
#include "Items/ItemRecords.hpp"

namespace VS::Menu::Inventory {
void SortLiveWeapons(int stat);
void SortLiveShields(int stat);
void SortLiveMisc(int stat);
void CopyLiveEquipmentStats(VS::Items::EquipmentPart* target, int category,
                            int index);
void SortLiveEquipment(int category, int stat);
}  // namespace VS::Menu::Inventory
