#pragma once

struct Items_WeaponLoadout;
struct Items_ShieldLoadout;

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
// Borrowed MENU4 status text, MAINMENU navigation/item text and BATTLE menu
// workspace. No persistent fields or allocations: overlay reload and cold
// reset remain with their existing owners, with no reset hook for this packet.
int NavigateLiveDetailElements(unsigned int selected, int flags);
void HoistLiveEquipmentRow(int row);
void BeginLiveDetailTransition(int row);
void InitializeLiveDetailPanel(int row, int class_affinity);
void SetLiveWeaponComponentRow(int row, Items_WeaponLoadout* weapon, int init);
void SetLiveShieldGemRow(int row, Items_ShieldLoadout* shield, int init);
void ExitLiveEquipmentDetails(int clear_dp_pp);
}  // namespace VS::Menu::Status
