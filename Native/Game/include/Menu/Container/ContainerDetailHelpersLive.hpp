#pragma once

struct Items_WeaponLoadout;
struct Items_ShieldLoadout;
struct Items_InventoryWeapon;
struct Items_InventoryShield;
struct Items_ContainerData;

namespace VS::Menu::container {
void BuildLiveWeaponStats(Items_WeaponLoadout* target,
                          Items_InventoryWeapon* source,
                          Items_ContainerData* container);
void BuildLiveShieldStats(Items_ShieldLoadout* target,
                          Items_InventoryShield* source,
                          Items_ContainerData* container);
void SetLivePanelVisibility(char visibility);
[[nodiscard]] int PollLiveDetailsPaging(int category, int page);
void BeginLiveDetailsTransition(int selected_row);
void InitializeLiveDetailsLayout(int selected_row, int stat_pages);
[[nodiscard]] int SelectLiveDetailsItem(int category, int page);
void ExitLiveDetailsView(int hide_dp_pp);
}  // namespace VS::Menu::container
