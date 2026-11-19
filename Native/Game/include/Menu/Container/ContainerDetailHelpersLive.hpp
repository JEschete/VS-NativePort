#pragma once
#include "Items/ItemRecords.hpp"

struct Items_WeaponLoadout;
struct Items_ShieldLoadout;

namespace VS::Menu::container {
void BuildLiveWeaponStats(Items_WeaponLoadout* target,
                          const VS::Items::InventoryWeapon* source,
                          VS::Items::ContainerData* container);
void BuildLiveShieldStats(Items_ShieldLoadout* target,
                          const VS::Items::InventoryShield* source,
                          VS::Items::ContainerData* container);
void SetLivePanelVisibility(char visibility);
[[nodiscard]] int PollLiveDetailsPaging(int category, int page);
void BeginLiveDetailsTransition(int selected_row);
void InitializeLiveDetailsLayout(int selected_row, int stat_pages);
[[nodiscard]] int SelectLiveDetailsItem(int category, int page);
void ExitLiveDetailsView(int hide_dp_pp);
}  // namespace VS::Menu::container
