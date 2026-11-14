#pragma once

struct Items_EquipmentPartStats;

namespace VS::Menu::Inventory {
void SortLiveWeapons(int stat);
void SortLiveShields(int stat);
void SortLiveMisc(int stat);
void CopyLiveEquipmentStats(Items_EquipmentPartStats* target, int category,
                            int index);
void SortLiveEquipment(int category, int stat);
}  // namespace VS::Menu::Inventory
